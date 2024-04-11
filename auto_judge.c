#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/time.h>
#include <fcntl.h>
#include <dirent.h>

int pipes_stdout[2]; //stdout �� ����� ������
int pipes_stderr[2]; //stderr�� ����� ������
int file_count = 0; //������ ���
int success=0;  //������ ���
int runtime_err=0;  //������ ���
int fail=0; //������ ���
int wrong_ans_num[21]; //������ ���
int rumtime_err_num[21]; //������ ���
int time_out_num[21]; //������ ���
int time_out_count=0; //������ ���
double total_time=0; //������ ���
int time_limit_grobal=0; //time_out ����

void check(char* answer_file, char* answer,int num){//��°� ������ ����(answer ������ Ư�� ���� �ּҸ� �޾ƿͼ�)
    FILE *file = fopen(answer_file, "r");
    if (file == NULL) {
        perror("���� ���� ����");
    }

    char file_buf[1024]="";
    size_t read_size = fread(file_buf, sizeof(char), sizeof(file_buf), file);
    fclose(file);

    if (read_size == 0) {
        perror("���� �б� ����");
    }

    file_buf[read_size+1] = '\0'; 
    if (strcmp(answer, file_buf) == 0) {
        printf("\033[0;32m");
        printf("����\n");
        success++;
        printf("\033[0m");
    } else {
        printf("\033[0;31m");
        printf("����\n");
        fail++;
        wrong_ans_num[num]++;
        printf("\033[0m");
    }
}

void alarm_handler(int signum) {//���� �� ���α׷� ����
    if (signum == SIGALRM) {
        exit(EXIT_FAILURE);
    }
}

void child_proc1(char* input, char* test){//�ð� �ʰ�Ȯ�� ���� alarm��� �� �׽�Ʈ ���α׷� ����
    int fd=-1;

    close(pipes_stderr[0]);
    close(pipes_stdout[0]);
    
    size_t len1 = strlen(input);
    size_t len2 = strlen(test);

    char* input_case = (char*)malloc(len1 + len2 + 1);
    if (input_case == NULL) {
        fprintf(stderr, "�޸� �Ҵ� ����\n");
        exit(EXIT_FAILURE);
    }

    strcpy(input_case, input);
    strcat(input_case, test); // ���� : ���丮 ��ο� ���� ��ΰ� �̾� �ٿ��� ��� ���� ������(//) �Է��� ����� ex) /home/nnz/inputdir//1.txt

    char *temp = NULL;
    if((temp = strstr(input_case, "//")) != NULL) { // input_case���� ���� ������(//)�� �߰��� ��� //�� /�� ����
        test = test + 1; // test�� ����Ű�� ���ڿ��� /1.txt���� 1.txt�� ����
        len2 = strlen(test);

        free(input_case); // ������ input_case�� ���� �Ҵ�� �޸𸮸� �Ҵ� ����
        char* input_case = (char*)malloc(len1 + len2 + 1); // input_case�� �޸� ���Ҵ�
        strcpy(input_case, input);
        strcat(input_case, test);
    }

    fd = open(input_case, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    free(input_case);

    dup2(pipes_stdout[1],1);
    dup2(pipes_stderr[1],2);
    dup2(fd,0);
    close(fd);
    
    signal(SIGALRM, alarm_handler);
    alarm(time_limit_grobal);
    execl("./pathname", "source", (char *) 0x0);
}

void parent_proc1(char* answer,char* test_case,int num){//�׽�Ʈ ���̽��� ���� ���� ����(1.���� �ð� 2. ����/���� 3. ���� 4. �ð� �ʰ�)
    ssize_t s;
    char buf[1024]="";
    char buf2[1024]="";
    int output=1;
    int err1=0;
    int time_limit = time_limit_grobal;
    int status;

    close(pipes_stderr[1]);
    close(pipes_stdout[1]);

    struct timeval start, end;
    double running_time;
    gettimeofday(&start, NULL);
    wait(&status);
    while ((s = read(pipes_stderr[0], buf, sizeof(buf) - 1)) > 0) {
        buf[s] = '\0';
        if(err1==0) {
            printf("\033[0;31m");
            printf("����>>>>>>\n");
            printf("\033[0m");
            }
        printf("%s\n", buf);
        err1=1;
    }

    while ((s = read(pipes_stdout[0], buf2, sizeof(buf) - 1)) > 0) {
        buf2[s] = '\0';
        output=0;
    }
    gettimeofday(&end, NULL);

    running_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    
    size_t len1 = strlen(answer);//answer���� �̸�
    size_t len2 = strlen(test_case);//�׽�Ʈ input ���� �̸�

    char* check_answer = (char*)malloc(len1 + len2 + 1);
    if (check_answer == NULL) {
        exit(EXIT_FAILURE);
    }

    strcpy(check_answer, answer);
    strcat(check_answer, test_case); // EX) "./input/4.txt" �� �����

    if(output==0) {
        check(check_answer,buf2,num);
        printf("���α׷� ���� �ð�: %f ��\n", running_time);
        total_time+=running_time;
    }
    else if(output==1){
        if(status==0){
            printf("\033[0;33m");
            printf("���� �ð��� �ʰ��Ǿ����ϴ�.\n");
            printf("\033[0m");
            time_out_num[num]++;
            time_out_count++;
        }
        else if(status>0){
            rumtime_err_num[num]++;
            runtime_err++;
        }
    }

    free(check_answer);
}

void start_test(char* input, char* answer, char* test_case, int num){//�� �׽�Ʈ ���̽� ���� fork�ؼ� ����
    int child_pid;
    printf("***************************************************************************\n");
    printf("%s ����\n",test_case);
    printf("***************************************************************************\n\n");
    if(pipe(pipes_stderr) != 0){
        perror("Errore");
        exit(EXIT_FAILURE);
    }
    if(pipe(pipes_stdout) != 0){
        perror("Errore");
        exit(EXIT_FAILURE);
    }

    if(child_pid=fork()){
        parent_proc1(answer,test_case,num);
    }
    else{
        child_proc1(input,test_case); 
    }

    printf("\n***************************************************************************\n\n\n");
}

void child_proc(char * c_source){//gcc�����ؼ� ������
    close(pipes_stderr[0]);
    dup2(pipes_stderr[1],2);
    execl("/usr/bin/gcc", "/usr/bin/gcc",
        "-fsanitize=address",
        c_source,
        "-o",
        "pathname",
        (char *) 0x0);
            perror("execl"); // ���� : execl �Լ��� ������ ����� ���� ó�� �߰�
            exit(EXIT_FAILURE);
}

void parent_proc(char* input, char* answer){//������ Ȯ�� �� ������ �� test����
    char buf[1024]="";
    ssize_t s;
    int err=0;
    pid_t child;

    close(pipes_stderr[1]);

    while ((s = read(pipes_stderr[0], buf, sizeof(buf) - 1)) > 0) {
        buf[s] = '\0'; 
        if(err==0) printf("������ ����\n");
        printf("\033[0;31m");
        printf("%s\n", buf);
        printf("\033[0m");
        err=1;
    }

    if (s < 0) {
        perror("�б� ����\n");
        exit(EXIT_FAILURE); // ���� : exit(0) -> exit(EXIT_FAILURE)�� ����
    }

    if (err == 0) {
        printf("������ ����\n");
        char test_case[256];
        for (int i = 1; i <=file_count; i++) {
        snprintf(test_case, sizeof(test_case), "/%d.txt", i);
        start_test(input, answer, test_case, i);
        }
    }
    else if(err>0){
        //printf("**************\n");
        //printf("������ ����\n");
        //printf("**************\n");
        exit(EXIT_FAILURE);
    } 

}

int main(int argc, char *argv[]){
    int opt;
    char *input= NULL, *answer=NULL, *time_limit=NULL, *c_source=NULL;

    while((opt = getopt(argc,argv,"i:a:t:")) != -1){
        switch (opt)
        {
        case 'i':
            input = optarg;
            break;
        case 'a':
            answer = optarg;
            break;
        case 't':
            time_limit = optarg;
            break;
        default:
            printf("Try './auto_judge -i <input_dir> -a <answer_dir> -t <time_out> <target_source>'\n\n");
            exit(1);
            break;
        }
    }

    for(int i = optind; i < argc; i++)
	{
		c_source = argv[i];
	}

    if (input != NULL && answer != NULL && time_limit != 0 && c_source != NULL) {
        printf("���͸�, Ŀ�ǵ� Ȯ��\n");
    }
    else{
        printf("Try './auto_judge -i <input_dir> -a <answer_dir> -t <time_out> <target_source>'\n\n"); // ���� : -s ���� 
        exit(1);
    }

    if (access(input, F_OK) == -1) {
        if (errno == ENOENT) {
            printf("�Է� ���丮 '%s'�� �������� �ʽ��ϴ�.\n\n", input);
        } else {
            perror("access");
        }
        exit(EXIT_FAILURE);
    }

    if (access(answer, F_OK) == -1) {
        if (errno == ENOENT) {
            printf("�Է� ���丮 '%s'�� �������� �ʽ��ϴ�.\n\n", answer);
        } else {
            perror("access");
        }
        exit(EXIT_FAILURE);
    }

    time_limit_grobal=atoi(time_limit);
    if(time_limit_grobal<1){
        printf("'-t' ���� 1 �̻��� ������ �Է��ϼ���.\n"); // ���� : "0 �̻�" -> "1 �̻�"���� ����
        exit(EXIT_FAILURE);
    }

    printf("ü�� ����\n");

    DIR *dir;
    struct dirent *entry;

    dir = opendir(input);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_count++;
        }
    }

    closedir(dir);

    pid_t  child_pid;
    int exit_code;

    if(pipe(pipes_stderr) != 0){
        perror("Errore");
        exit(EXIT_FAILURE);
    }

    if(child_pid=fork()){
        parent_proc(input, answer);
    }
    else{
        child_proc(c_source);
    }

    wait(&exit_code);

    printf("|||���|||   time_limit: %d��\n �� �׽�Ʈ: %d��\n ����: %d��\n ����: %d��",time_limit_grobal,file_count,success,fail);
    int c=0;
    if(fail){
        printf("   ||");
        for(c=0;c<21;c++){
            if(wrong_ans_num[c]){
                printf("%d��,",c);
            }
        }
    }
    printf("\n ���� / �� �׽�Ʈ: %f\n ��Ÿ�� ����: %d��",(double)success/(double)file_count,runtime_err);
    if(runtime_err){
        printf("   ||");
        for(c=0;c<21;c++){
            if(rumtime_err_num[c]){
                printf("%d��,",c);
            }
        }
    }
    printf("\n Ÿ�� �ƿ�: %d��",time_out_count);
    if(time_out_count){
        printf("   ||");
        for(c=0;c<21;c++){
            if(time_out_num[c]){
                printf("%d��,",c);
            }
        }
    }
    printf("\n ������ ������ �� ���� �ð�: %f��\n",total_time);

    exit(EXIT_SUCCESS);
    }