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

int pipes_stdout[2]; //stdout 로 사용할 파이프
int pipes_stderr[2]; //stderr로 사용할 파이프
int file_count = 0; //마지막 출력
int success=0;  //마지막 출력
int runtime_err=0;  //마지막 출력
int fail=0; //마지막 출력
int wrong_ans_num[21]; //마지막 출력
int rumtime_err_num[21]; //마지막 출력
int time_out_num[21]; //마지막 출력
int time_out_count=0; //마지막 출력
double total_time=0; //마지막 출력
int time_limit_grobal=0; //time_out 설정

void check(char* answer_file, char* answer,int num){//출력과 정답을 비교함(answer 폴더의 특정 파일 주소를 받아와서)
    FILE *file = fopen(answer_file, "r");
    if (file == NULL) {
        perror("파일 열기 실패");
    }

    char file_buf[1024]="";
    size_t read_size = fread(file_buf, sizeof(char), sizeof(file_buf), file);
    fclose(file);

    if (read_size == 0) {
        perror("파일 읽기 실패");
    }

    file_buf[read_size+1] = '\0'; 
    if (strcmp(answer, file_buf) == 0) {
        printf("\033[0;32m");
        printf("정답\n");
        success++;
        printf("\033[0m");
    } else {
        printf("\033[0;31m");
        printf("오답\n");
        fail++;
        wrong_ans_num[num]++;
        printf("\033[0m");
    }
}

void alarm_handler(int signum) {//실행 시 프로그램 종료
    if (signum == SIGALRM) {
        exit(EXIT_FAILURE);
    }
}

void child_proc1(char* input, char* test){//시간 초과확인 위한 alarm등록 후 테스트 프로그램 실행
    int fd=-1;

    close(pipes_stderr[0]);
    close(pipes_stdout[0]);
    
    size_t len1 = strlen(input);
    size_t len2 = strlen(test);

    char* input_case = (char*)malloc(len1 + len2 + 1);
    if (input_case == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        exit(EXIT_FAILURE);
    }

    strcpy(input_case, input);
    strcat(input_case, test); // 준혁 : 디렉토리 경로와 파일 경로가 이어 붙여질 경우 더블 슬래시(//) 입력이 우려됨 ex) /home/nnz/inputdir//1.txt

    char *temp = NULL;
    if((temp = strstr(input_case, "//")) != NULL) { // input_case에서 더블 슬래시(//)를 발견할 경우 //을 /로 수정
        test = test + 1; // test가 가리키는 문자열을 /1.txt에서 1.txt로 변경
        len2 = strlen(test);

        free(input_case); // 기존에 input_case에 동적 할당된 메모리를 할당 해제
        char* input_case = (char*)malloc(len1 + len2 + 1); // input_case에 메모리 재할당
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

void parent_proc1(char* answer,char* test_case,int num){//테스트 케이스에 따른 정보 수집(1.실행 시간 2. 정답/오답 3. 오류 4. 시간 초과)
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
            printf("오류>>>>>>\n");
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
    
    size_t len1 = strlen(answer);//answer폴더 이름
    size_t len2 = strlen(test_case);//테스트 input 파일 이름

    char* check_answer = (char*)malloc(len1 + len2 + 1);
    if (check_answer == NULL) {
        exit(EXIT_FAILURE);
    }

    strcpy(check_answer, answer);
    strcat(check_answer, test_case); // EX) "./input/4.txt" 를 만들어

    if(output==0) {
        check(check_answer,buf2,num);
        printf("프로그램 실행 시간: %f 초\n", running_time);
        total_time+=running_time;
    }
    else if(output==1){
        if(status==0){
            printf("\033[0;33m");
            printf("실행 시간이 초과되었습니다.\n");
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

void start_test(char* input, char* answer, char* test_case, int num){//각 테스트 케이스 마다 fork해서 실행
    int child_pid;
    printf("***************************************************************************\n");
    printf("%s 실행\n",test_case);
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

void child_proc(char * c_source){//gcc실행해서 컴파일
    close(pipes_stderr[0]);
    dup2(pipes_stderr[1],2);
    execl("/usr/bin/gcc", "/usr/bin/gcc",
        "-fsanitize=address",
        c_source,
        "-o",
        "pathname",
        (char *) 0x0);
            perror("execl"); // 준혁 : execl 함수가 실패할 경우의 에러 처리 추가
            exit(EXIT_FAILURE);
}

void parent_proc(char* input, char* answer){//컴파일 확인 후 정상일 때 test진행
    char buf[1024]="";
    ssize_t s;
    int err=0;
    pid_t child;

    close(pipes_stderr[1]);

    while ((s = read(pipes_stderr[0], buf, sizeof(buf) - 1)) > 0) {
        buf[s] = '\0'; 
        if(err==0) printf("컴파일 오류\n");
        printf("\033[0;31m");
        printf("%s\n", buf);
        printf("\033[0m");
        err=1;
    }

    if (s < 0) {
        perror("읽기 에러\n");
        exit(EXIT_FAILURE); // 준혁 : exit(0) -> exit(EXIT_FAILURE)로 수정
    }

    if (err == 0) {
        printf("컴파일 성공\n");
        char test_case[256];
        for (int i = 1; i <=file_count; i++) {
        snprintf(test_case, sizeof(test_case), "/%d.txt", i);
        start_test(input, answer, test_case, i);
        }
    }
    else if(err>0){
        //printf("**************\n");
        //printf("컴파일 오류\n");
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
        printf("디렉터리, 커맨드 확인\n");
    }
    else{
        printf("Try './auto_judge -i <input_dir> -a <answer_dir> -t <time_out> <target_source>'\n\n"); // 준혁 : -s 삭제 
        exit(1);
    }

    if (access(input, F_OK) == -1) {
        if (errno == ENOENT) {
            printf("입력 디렉토리 '%s'가 존재하지 않습니다.\n\n", input);
        } else {
            perror("access");
        }
        exit(EXIT_FAILURE);
    }

    if (access(answer, F_OK) == -1) {
        if (errno == ENOENT) {
            printf("입력 디렉토리 '%s'가 존재하지 않습니다.\n\n", answer);
        } else {
            perror("access");
        }
        exit(EXIT_FAILURE);
    }

    time_limit_grobal=atoi(time_limit);
    if(time_limit_grobal<1){
        printf("'-t' 에는 1 이상의 정수를 입력하세요.\n"); // 준혁 : "0 이상" -> "1 이상"으로 수정
        exit(EXIT_FAILURE);
    }

    printf("체점 시작\n");

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

    printf("|||결과|||   time_limit: %d초\n 총 테스트: %d개\n 정답: %d개\n 오답: %d개",time_limit_grobal,file_count,success,fail);
    int c=0;
    if(fail){
        printf("   ||");
        for(c=0;c<21;c++){
            if(wrong_ans_num[c]){
                printf("%d번,",c);
            }
        }
    }
    printf("\n 정답 / 총 테스트: %f\n 런타임 오류: %d개",(double)success/(double)file_count,runtime_err);
    if(runtime_err){
        printf("   ||");
        for(c=0;c<21;c++){
            if(rumtime_err_num[c]){
                printf("%d번,",c);
            }
        }
    }
    printf("\n 타임 아웃: %d개",time_out_count);
    if(time_out_count){
        printf("   ||");
        for(c=0;c<21;c++){
            if(time_out_num[c]){
                printf("%d번,",c);
            }
        }
    }
    printf("\n 오류를 제외한 총 실행 시간: %f초\n",total_time);

    exit(EXIT_SUCCESS);
    }