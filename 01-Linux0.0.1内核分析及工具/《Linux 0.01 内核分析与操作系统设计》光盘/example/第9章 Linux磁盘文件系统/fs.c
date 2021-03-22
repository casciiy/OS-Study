/* fs.c */
#include    <stdio.h>
#include    <conio.h>


#define     FILE_NUM        10
#define     FILE_SIZE       (1024*10)
#define     PUT_PROMPT      printf("FS#")

const       char    file_system_name[] = "fs.dat";

FILE    *fp;

struct inode{
    char file_name[512];
    int file_length;
};

struct  inode   *p;

struct inode inode_array[FILE_NUM];

void creat_file_system(){
    long len;
    int inode_num;
    int i;

    fp=fopen(file_system_name,"wb");
    if(fp==NULL){
        printf("Create file error!\n");
        exit(1);
    }

    for(len=0;len< (sizeof(inode_array[0])+FILE_SIZE)*FILE_NUM;len++){
        fputc(0,fp);
    }

    for(i=0;i<FILE_NUM;i++){
        strcpy(inode_array[i].file_name,"");
        inode_array[i].file_length =0;
        p=&inode_array[i];
        fwrite(p,sizeof(inode_array[0]),1,fp);
    }

    fflush(fp);
}

void open_file_system(){
    int i;

    fp=fopen(file_system_name,"r");
    if(fp==NULL){
        creat_file_system();
    }

    fp=fopen(file_system_name,"r+");
    if(fp==NULL){
        printf("Open file to read/write error!\n");
        exit(1);
    }

    p = &inode_array[0];
    fseek(fp,0,SEEK_SET);
    fread(p,sizeof(inode_array[0])*FILE_NUM,1,fp);
}


int new_a_file(char *file_name){
    int i;
    for(i=0;i<FILE_NUM;i++){
        if(strcmp(inode_array[i].file_name,"")==0){
            strcpy(inode_array[i].file_name,file_name);
            p = &inode_array[i];
            fseek(fp,sizeof(inode_array[0])*i,SEEK_SET);
            if(fwrite(p,sizeof(inode_array[0]),1,fp)!=1){
                printf("new a file error!\n");
                exit(1);
            }
            fflush(fp);
            return i;
        }
    };
    return -1;
}

int del_a_file(char *file_name){
    int i;
    for(i=0;i<FILE_NUM;i++){
        if(strcmp(inode_array[i].file_name,file_name)==0){
            strcpy(inode_array[i].file_name,"");
            p = &inode_array[i];
            fseek(fp,sizeof(inode_array[0])*i,SEEK_SET);
            fwrite(p,sizeof(inode_array[0]),1,fp);
            fflush(fp);
            return i;
        }
    };
    return -1;
}

void list(){
    int i;
    int count;

    printf("\n");
    count=0;
    for(i=0;i<FILE_NUM;i++){
        if(strcmp(inode_array[i].file_name,"")!=0){
            printf("\tFile name: %s \t\t\t [%d]\n",inode_array[i].file_name,
                   inode_array[i].file_length);
            count++;
        }
    };
    printf("\tFiles count = %d\n",count);
}

int open_a_file(char *file_name){
    int i;

    for(i=0;i<FILE_NUM;i++){
        if(strcmp(inode_array[i].file_name,file_name)==0){
            return i;
        }
    };
}

int offset_by_i(int i){
    return sizeof(inode_array[0])*FILE_NUM + FILE_SIZE*i;
}

int write(char *file_name,int offset,char *str,int count){
    int handle;

    handle = open_a_file(file_name);
    fseek(fp,offset_by_i(handle)+offset,SEEK_SET);
    fwrite(str,count,1,fp);

    inode_array[handle].file_length = strlen(str)+offset;
    p = &inode_array[handle];
    fseek(fp,sizeof(inode_array[0])*handle,SEEK_SET);
    fwrite(p,sizeof(inode_array[0]),1,fp);
    fflush(fp);

}

int read(char *file_name,int offset,int count,char *str){
    int handle;
    char buf[FILE_SIZE];

    handle = open_a_file(file_name);
    fseek(fp,offset_by_i(handle)+offset,SEEK_SET);
    fread(buf,count,1,fp);
    strncpy(str,buf,count);
}

void print_help()
{
    printf("Please select: 1. Creat a new file system\n");
    printf("               2. Make a new file\n");
    printf("               3. Del a file\n");
    printf("               4. List files\n");
    printf("               5. Write a string to a file\n");
    printf("               6. Read a string from a file\n");
    printf("               7. Exit\n");
    printf("                                              \n");
    printf("               h for help\n");
}

int main()
{
    char buf1[FILE_SIZE];
    char key;
    char buf2[5120];

    clrscr();

    print_help();

    key = '0';
    open_file_system();
    while(key!='7')
    {
        PUT_PROMPT;
        key=getch();
        putch(key);
        strcpy(buf1,"");
        strcpy(buf2,"");
        switch(key)
        {
        case '1':
            fclose(fp);
            creat_file_system();
            printf("\nCreate file system succeed!\n");
            open_file_system();
            break;
        case '2':
            puts("\nPlease input a file name:");
            scanf("%s",buf1);
            new_a_file(buf1);
            printf("Add a file succeed!\n");
            break;
        case '3':
            puts("\nPlease input a file name:");
            scanf("%s",buf1);
            del_a_file(buf1);
            printf("Del file %s succeed!\n",buf1);
            break;
        case '4':
            list();
            break;
        case '5':
            puts("\nPlease input a file name:");
            scanf("%s",buf1);
            puts("\nPlease input a string:");
            scanf("%s",buf2);
            write(buf1,0,buf2,strlen(buf2)+1);
            printf("\nWrite a file succeed!\n");
            break;
        case '6':
            puts("\nPlease input a file name:");
            scanf("%s",buf2);
            read(buf2,0,FILE_SIZE,buf1);
            puts(buf1);
            break;
        case 'h':
            printf("\n\n");
            print_help();
            break;
        case '7':
            break;
        default:
            printf("\n");

        }

    }
    return 0;
}
