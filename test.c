#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define DELETED 0xE5
#define ARCHIVE 0x20
#define SUBFOLDER 0x10
#define LONG_FILENAME 0x0f
#define VOLUME_LABEL 0x08
#define SYSTEM_FILE 0x04
#define HIDDEN_FOLDER 0x02
#define READ_ONLY 0x01
#define UNALLOCATED 0x00

#define ROK 0xfe00
#define MIESIAC 0x01e0
#define DZIEN 0x001f

#define GODZINY 0xf800
#define MINUTY 0x07e0
#define SEKUNDY 0x001f

struct __attribute__((packed)) bSector
{
    uint8_t  jumps[3];
    uint8_t  OEM_IDENTIFIER[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved;
    uint8_t  FAT_count;
    uint16_t maximum_files_root;
    uint16_t sectors16_count;
    uint8_t  media_type;
    uint16_t size_of_FAT; 
    uint16_t sectors_per_track;                      
    uint16_t num_of_heads;
    uint32_t num_of_sectors_before_sp;
    uint32_t sectors32_count;
    uint8_t  drive_num;
    uint8_t  empty;
    uint8_t  ext_boot_signature;
    uint32_t vol_serial;
    uint8_t  vol_label[11];
    uint8_t  system_type_level[8];
    uint8_t  empty_tab[448];
    uint16_t signature;
};

struct __attribute__((packed)) dFile
{
    union 
    {
        uint8_t allocation_status;
        struct 
        {
            uint8_t filename[8];
            uint8_t extension[3];
        };
    };
    uint8_t  attrs;
    uint8_t  reserved;
    uint8_t  create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_address_high;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t first_cluster_address_low;
    uint32_t size;
};

struct cd_stack {
    struct dFile * stack[100];
    unsigned int current_stack_size;
};

int count_clusters(struct dFile * file, unsigned char * fat, struct bSector * boot);

int main(void)
{
    char * nazwa = "default.img";
    FILE * obraz = fopen(nazwa, "rb");
    if (!obraz)
    {
        printf("Blad otwarcia obrazu dysku");
        return 1;
    }
    struct bSector boot;
    fread(&boot, sizeof(boot), 1, obraz);
    unsigned int rozmiar_fat = boot.FAT_count * (boot.size_of_FAT * boot.bytes_per_sector);
    unsigned char * fat = (unsigned char *)malloc(rozmiar_fat);
    fread(fat, rozmiar_fat, 1, obraz);
    if (!fat)
    {
        printf("Blad alokacji FAT");
        return 1;
    }
    unsigned int rozmiar_root = boot.maximum_files_root * sizeof(struct dFile);
    struct dFile * root = (struct dFile *)malloc(rozmiar_root);
    if (!root)
    {
        printf("Blad alokacji root");
        return 1;
    }
    fread(root, rozmiar_root, 1, obraz);
    unsigned int sektory = boot.sectors16_count;
    if (!sektory) sektory = boot.sectors32_count;
    unsigned int rozmiar_data = (sektory * boot.bytes_per_sector) - (rozmiar_root + rozmiar_fat + sizeof(boot));
    char * data = (char *)malloc(rozmiar_data);
    if (!data)
    {
        printf("Blad alokacji data");
        return 1;
    }
    fread(data, rozmiar_data, 1, obraz);
    char * linia = malloc(200);
    if (!linia)
    {
        printf("Blad alokacji pamieci dla lini");
        return 1;
    }
    struct cd_stack stack;
    stack.current_stack_size = 0;
    printf("dir -> wypisuje pliki i foldery\n");
    printf("pwd -> wyswietla obecna sciezke\n");
    printf("exit - wyjscie z programu\n");
    printf("rootinfo - pokazuje informacje na temat ROOT\n");
    printf("spaceinfo - pokazuje informacje o klastrach FAT\n");
    printf("fileinfo @nazwa -> wyswietla informacje o podanym pliku @nazwa\n");
    printf("cd @nazwa -> przejscie do podfolderu @nazwa\n");
    printf("cat @nazwa -> listuje zawartosc pliku danego argumentem @nazwa\n");
    printf("get @nazwa -> kopiuje plik @nazwa do ROOTa\n");
    printf("zip @nazwa @nazwa2 @nazwa3 -> konkatenuje pliki @nazwa oraz @nazwa2 do pliku @nazwa3 zapisywanego w ROOT \n");
    while (1)
    {
        for (int i = 0; i < 200; i++) linia[i] = 0;
        printf("Podaj polecenie: ");
        fgets(linia, 200, stdin);
        printf("Podane polecenie: %s", linia);
        char polecenia[4][50];
        char * part = strtok(linia, " \n");
        int i = 0;
        while (part != NULL)
        {
            if (i > 3) break;
            memcpy(polecenia[i], part, 50);
            part = strtok(NULL, " \n");
            i++;
        }
        if (!strcmp(polecenia[0], "dir"))
        {
            if (!stack.current_stack_size)
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        uint16_t godzina = ((root + i)->create_time & GODZINY) >> 11;
                        uint16_t minuta = ((root + i)->create_time & MINUTY) >> 5;
                        uint16_t sekunda = ((root + i)->create_time & SEKUNDY) * 2;
                        uint16_t rok = 1980 + (((root + i)->create_date & ROK) >> 9);
                        uint16_t miesiac = (((root + i)->create_date & MIESIAC) >> 5);
                        uint16_t dzien = ((root + i)->create_date & DZIEN);
                        printf("Data utworzenia: %02hd:%02hd:%02hd || ", godzina, minuta, sekunda);
                        printf("%hd/%hd/%hd\t", dzien, miesiac, rok);
                        if ((root + i) ->attrs & SUBFOLDER) printf("FOLDER  ");
                        else printf("PLIK rozmiar: %d  ", (root + i) -> size);
                        printf("Nazwa: %s\n", (root + i)->filename);
                    }
                }
            }
            else 
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size-1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector * boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != DELETED && (dir_chain+j)->filename[0] != UNALLOCATED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        if ((root + j)->attrs & HIDDEN_FOLDER) continue;
                        uint16_t godzina = ((dir_chain+j)->create_time & GODZINY) >> 11;
                        uint16_t minuta = ((dir_chain+j)->create_time & MINUTY) >> 5;
                        uint16_t sekunda = ((dir_chain+j)->create_time & SEKUNDY) * 2;
                        uint16_t rok = 1980 + (((dir_chain+j)->create_date & ROK) >> 9);
                        uint16_t miesiac = (((dir_chain+j)->create_date & MIESIAC) >> 5);
                        uint16_t dzien = ((dir_chain+j)->create_date & DZIEN);
                        printf("Data utworzenia: %02hd:%02hd:%02hd || ", godzina, minuta, sekunda);
                        printf("%hd/%hd/%hd\t", dzien, miesiac, rok);
                        if ((dir_chain+j)->attrs & SUBFOLDER) printf("FOLDER  ");
                        else printf("PLIK rozmiar: %d  ", (dir_chain+j) -> size);
                        printf("Nazwa: %s\n", (dir_chain+j)->filename);                    
                    }

                }
                free(all_data);
            }
        }
        else if (!strcmp(polecenia[0], "pwd"))
        {
            printf("Root:\\");
            for (int i = 1; i <= stack.current_stack_size; i++)
            {
                struct dFile * temp = stack.stack[i - 1];
                char * token = strtok(temp->filename," \n");
                printf("%s\\", token);
            }
            printf("\n");
        }
        else if (!strcmp(polecenia[0], "exit"))
        {
            free(fat);
            free(root);
            free(data);
            return 0;
        }
        else if (!strcmp(polecenia[0], "rootinfo"))
        {
            int i = 0;
            int wpisy = 0;
            while (i < boot.maximum_files_root)
            {
                if ((*(root + i)).filename[0] != UNALLOCATED)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).attrs != LONG_FILENAME) wpisy++;
                }
                i++;
            }
            printf("Istniejace wpisy w katalogu glownym: %d\n", wpisy);
            printf("Maksimum wpisow w katalogu glownym: %d\n", boot.maximum_files_root);
            float val = ((float)wpisy / (float)boot.maximum_files_root) * 100;
            printf("Zajeta przestrzen w katalogo glownym: %.1f%%\n", val);
        }
        else if (!strcmp(polecenia[0], "spaceinfo"))
        {
            unsigned int klastry_zajete = 0, klastry_wolne = 0, klastry_zps = 0, klastry_eoc = 0;
            printf("Rozmiar klastra w sektorach: %u\n", boot.sectors_per_cluster);
            printf("Rozmiar klastra w bajtach: %u\n", boot.bytes_per_sector * boot.sectors_per_cluster);
            int size = ((boot.size_of_FAT * boot.bytes_per_sector)/3)*2;
            for (int i = 0; i < size; i++)
            {
                uint16_t wpis = i;
                uint32_t fat_off = wpis + (wpis / 2);
                uint16_t val = wpis;
                wpis = *(uint16_t *)(fat + fat_off);
                if (val & 0x0001) wpis = wpis >> 4;
                else wpis = wpis & 0x0FFF;
                if (wpis >= 0xFF8) klastry_eoc++;
                else if (wpis == 0x000) klastry_wolne++;
                else if (wpis == 0xFF7) klastry_zps++;
                else klastry_zajete++;
            }
            printf("Klastry wolne: %u\n", klastry_wolne);
            printf("Klastry zajete: %u\n", klastry_zajete);
            printf("Klastry end-of-chain %u\n", klastry_eoc);
            printf("Klastry zepsute: %u\n", klastry_zps);
        }
        else if (!strcmp(polecenia[0], "fileinfo"))
        {
            //---Sprawdzenie czy podana nazwa istnieje w ostatnim wpisie na stosie.
            struct dFile * plik = NULL;
            if (!stack.current_stack_size) //Root
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((root + i)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (root + i);
                            break;                
                        }                        
                    }
                }
            }
            else
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size - 1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != UNALLOCATED && (dir_chain+j)->filename[0] != DELETED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((dir_chain+j)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (dir_chain+j);
                            break;                
                        }                     
                    }
                }
                free(all_data);
            }
            if (plik != NULL)
            {
                printf("Root:\\");
                for (int i = 1; i <= stack.current_stack_size; i++)
                {
                    struct dFile * temp = stack.stack[i - 1];
                    char * token = strtok(temp->filename," \n");
                    printf("%s\\", token);
                }
                uint16_t data_stworzenia[6] = {1980 + (((plik)->create_date & ROK) >> 9),(((plik)->create_date & MIESIAC) >> 5),((plik)->create_date & DZIEN),(((plik)->create_time & GODZINY) >> 11),(((plik)->create_time & MINUTY) >> 5),(((plik)->create_time & SEKUNDY) * 2)};
                uint16_t data_modyfikacji[6] = {1980 + (((plik)->mod_date & ROK) >> 9),(((plik)->mod_date & MIESIAC) >> 5),((plik)->mod_date & DZIEN),(((plik)->mod_time & GODZINY) >> 11),(((plik)->mod_time & MINUTY) >> 5),(((plik)->mod_time & SEKUNDY) * 2)};
                uint16_t data_dostepu[3] = {1980 + (((plik)->access_date & ROK) >> 9),(((plik)->access_date & MIESIAC) >> 5),((plik)->access_date & DZIEN)};
                printf("\nAtrybut D: ");
                if (plik->attrs & SUBFOLDER) printf("+");
                else printf("-");
                printf("\nAtrybut H: ");
                if (plik->attrs & HIDDEN_FOLDER) printf("+");
                else printf("-");
                printf("\nAtrybut A: ");
                if (plik->attrs & ARCHIVE) printf("+");
                else printf("-");
                printf("\nAtrybut S: ");
                if (plik->attrs & SYSTEM_FILE) printf("+");
                else printf("-");
                printf("\nAtrybut R: ");
                if (plik->attrs & READ_ONLY) printf("+");
                else printf("-");
                printf("\nAtrybut V: ");
                if (plik->attrs & VOLUME_LABEL) printf("+\n");
                else printf("-\n");
                printf("Wielkosc pliku: %u\n", plik->size);
                printf("Data stworzenia: ");
                for (int i = 0; i < 6; i++)
                {
                    printf("%hd", data_stworzenia[i]);
                    if (i == 2) printf("  ");
                    if (i < 2) printf("/");
                    if (i > 2 && i != 5) printf(":");
                }
                printf("\nData modyfikacji: ");
                for (int i = 0; i < 6; i++)
                {
                    printf("%hd", data_stworzenia[i]);
                    if (i == 2) printf("  ");
                    if (i < 2) printf("/");
                    if (i > 2 && i != 5) printf(":");
                }
                printf("\nData dostepu: ");
                for (int i = 0; i < 3; i++)
                {
                    printf("%hd", data_dostepu[i]);
                    if (i != 2) printf(":");
                }
                printf("\nLancuch klastrow\n");
                printf("Poczatkowy klaster -> ");
                uint16_t tempAddress = plik->first_cluster_address_low;
                while (1)
                {    
                    printf("%hu ", tempAddress);
                    if (tempAddress >= 0xFF7) break;        
                    uint16_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                printf("<- Konczacy klaster\n");
            }
            else
            {
                printf("Podany plik nie istnieje w tym folderze\n");
            }

        }
        else if (!strcmp(polecenia[0], "cd"))
        {
            if (!strcmp(polecenia[1], ".."))
            {
                if (stack.current_stack_size > 0) stack.current_stack_size--;
                continue;
            }

            if (!stack.current_stack_size) //Root
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((root + i)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            stack.stack[stack.current_stack_size] = (root + i);
                            stack.current_stack_size++;
                            break;                
                        }                        
                    }
                }
            }
            else
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size - 1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != UNALLOCATED && (dir_chain+j)->filename[0] != DELETED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((dir_chain+j)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            stack.stack[stack.current_stack_size] = (dir_chain+j);
                            stack.current_stack_size++;
                            break;                
                        }                     
                    }
                }
                free(all_data);
            }
        }
        else if (!strcmp(polecenia[0], "cat"))
        {
            struct dFile * plik = NULL;
            if (!stack.current_stack_size) //Root
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((root + i)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (root + i);
                            break;                
                        }                        
                    }
                }
            }
            else
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size - 1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != UNALLOCATED && (dir_chain+j)->filename[0] != DELETED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((dir_chain+j)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (dir_chain + j);
                            break;                
                        }                     
                    }
                }
                free(all_data);
            }
            if (plik != NULL&& plik->size>0)
            {
                struct dFile * tempFile = plik;
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                printf("%s\n", all_data);
                free(all_data);
            }
            else
            {
                printf("Nie znaleziono pliku\n");
            }
        }
        else if (!strcmp(polecenia[0], "get"))
        {
            struct dFile * plik = NULL;
            if (!stack.current_stack_size) //Root
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((root + i)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (root + i);
                            break;                
                        }                        
                    }
                }
            }
            else
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size - 1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != UNALLOCATED && (dir_chain+j)->filename[0] != DELETED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((dir_chain+j)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (dir_chain + j);
                            break;                
                        }                     
                    }
                }
                free(all_data);
            }
            if (plik != NULL && plik->size>0)
            {
                struct dFile * tempFile = plik;
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                FILE * nPlik = fopen("wynik.txt", "w");
                if (!nPlik) 
                {
                    printf("Nie udalo sie otworzyc pliku\n"); 
                    free(all_data);
                }
                else
                {
                    fprintf(nPlik,"%s", all_data);
                    fclose(nPlik);
                    free(all_data);
                }
            }
            else
            {
                printf("Nie znaleziono pliku\n");
            }
        }
        else if (!strcmp(polecenia[0], "zip"))
        {
            struct dFile * plik = NULL;
            if (!stack.current_stack_size) //Root
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((root + i)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (root + i);
                            break;                
                        }                        
                    }
                }
            }
            else
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size - 1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != UNALLOCATED && (dir_chain+j)->filename[0] != DELETED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((dir_chain+j)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[1]); g++) polecenia[1][g] = toupper(polecenia[1][g]);
                        if (!strcmp(token, polecenia[1]))
                        {
                            plik = (dir_chain + j);
                            break;                
                        }                     
                    }
                }
                free(all_data);
            }
            struct dFile * plik2 = NULL;
            if (!stack.current_stack_size) //Root
            {
                for (int i = 0; i < boot.maximum_files_root; i++)
                {
                    if ((*(root + i)).filename[0] != DELETED && (*(root + i)).filename[0] != UNALLOCATED)
                    {
                        if ((*(root + i)).attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((root + i)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[2]); g++) polecenia[2][g] = toupper(polecenia[2][g]);
                        if (!strcmp(token, polecenia[2]))
                        {
                            plik2 = (root + i);
                            break;                
                        }                        
                    }
                }
            }
            else
            {
                struct dFile * tempFile = stack.stack[stack.current_stack_size - 1];
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                int liczba_plikow = (klastry * boot.bytes_per_sector * boot.sectors_per_cluster) / sizeof(struct dFile);
                struct dFile * dir_chain = (struct dFile *)all_data;
                for (int j = 0; j < liczba_plikow; j++)
                {
                    if ((dir_chain+j)->filename[0] != UNALLOCATED && (dir_chain+j)->filename[0] != DELETED)
                    {
                        if ((dir_chain+j)->attrs & HIDDEN_FOLDER) continue;
                        char * token = strtok((dir_chain+j)->filename," \n");
                        for (unsigned int g = 0; g < strlen(token); g++) token[g] = toupper(token[g]);
                        for (unsigned int g = 0; g < strlen(polecenia[2]); g++) polecenia[2][g] = toupper(polecenia[2][g]);
                        if (!strcmp(token, polecenia[2]))
                        {
                            plik2 = (dir_chain + j);
                            break;                
                        }                     
                    }
                }
                free(all_data);
            }
            if (plik2 != NULL && plik != NULL)
            {
                struct dFile * tempFile = plik;
                uint16_t tempAddress = tempFile->first_cluster_address_low;
                int klastry = count_clusters(tempFile, fat, &boot);
                int i = 0;
                char * all_data = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                FILE * nPlik = fopen(polecenia[3], "w");
                if (!nPlik) 
                {
                    printf("Nie udalo sie otworzyc pliku\n"); 
                    free(all_data);
                }
                else
                {
                    fwrite(all_data, plik->size, 1, nPlik);
                    free(all_data);
                }
                struct dFile * tempFile2 = plik2;
                tempAddress = tempFile2->first_cluster_address_low;
                klastry = count_clusters(tempFile2, fat, &boot);
                i = 0;
                char * all_data2 = (char *)malloc(klastry * boot.bytes_per_sector * boot.sectors_per_cluster);
                if (!all_data2) return 1;
                while (1)
                {    
                    uint32_t data_off = (tempAddress - 2) * (boot.bytes_per_sector* boot.sectors_per_cluster);
                    if (tempAddress >= 0xFF7) break;        
                    char *content_ptr = data + data_off;
                    for (int j = 0; j < boot.bytes_per_sector * boot.sectors_per_cluster; j++)
                    {
                        all_data[i] = content_ptr[j];
                        i++;
                    }
                    uint32_t fat_off = tempAddress + (tempAddress / 2);
                    uint16_t val = tempAddress;
                    tempAddress = *(uint16_t *)(fat + fat_off);
                    if (val & 0x0001) tempAddress = tempAddress >> 4;
                    else tempAddress = tempAddress & 0x0FFF;
                }
                fwrite(all_data2, plik2->size, 1, nPlik);
                free(all_data2);
                fclose(nPlik);
            }

        }
        else
        {
            printf("Bledne polecenie\n");
        }
    }
    return 0;
}

int count_clusters(struct dFile * file, unsigned char * fat, struct bSector * boot)
{
    if (!file) return 0;
    uint16_t tempAddress = file->first_cluster_address_low;
    int klastry = 0;
    while (1)
    {    
        if (tempAddress >= 0xFF7) break;        
        klastry++;
        uint16_t fat_off = tempAddress + (tempAddress / 2);
        uint16_t val = tempAddress;
        tempAddress = *(uint16_t *)(fat + fat_off);
        if (val & 0x0001) tempAddress = tempAddress >> 4;
        else tempAddress = tempAddress & 0x0FFF;
    }
    return klastry;
}
