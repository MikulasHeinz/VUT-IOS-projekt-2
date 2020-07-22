#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>

#define MAKEMAP(pointer) {(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
#define UNMAP(pointer) {munmap((pointer), sizeof((pointer)));}



sem_t *s_outside = NULL; //pruchod, pokud je soudce v venku
sem_t *s_inside = NULL; //pruchod, pokud je soudce v budove
sem_t *mutex_entering = NULL;
sem_t *mutex_checking = NULL;
sem_t *programStops = NULL;
sem_t *allRegistered = NULL;
sem_t *judgeHasDecided = NULL;
sem_t *forPrint = NULL;
sem_t *immEntry = NULL;
FILE *pfile;


//global variables
int pi; //pocet migrantu
int ig; //max doba generovani migranta
int jg; //max doba, kdy soudce vejde zpet do budovy
int it; //doba trvani vyzvedavani certifikatu
int jt; // doba trvani vydavani rozhodnuti

int immNum;
//print variables

int *line = 0;
int *judgeInside = 0;
int *ne = 0;
int *nc = 0;
int *nb = 0;
int *leftovers = 0;
int *rolling = 0;
int *conditionDecision = 0;
/////////////////////////declare functions/////////////////////////////////////////
//judge functions:
void processJudge();
void judgeEnters();
void judgeDecides();
void judgeLeaves();
void judgeFinishes();
//judge print
void judgePrint(char *thing);
void judgePrintShort(char *thing);
//immigrant functions:
void generateImmigrants(int pi, int ig);
void processImmigrant();
void immigrantEnters(int immNum);
void immChecks(int immNum);
void immWantsCertificate(int immNum);
void immLeaves(int immNum);
//immigrant print
void immPrint(int immNum, char *thing);
void immPrintShort(int immNum);
//overall functions
void mySleep(int max_time);
void useHelp();
int init();
void cleanup();
void help(char *string);
/////////////////////////////////////MAIN///////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
    pfile = fopen("proj2.out", "w");
    if(pfile == NULL){
        fputs("could not open file", stderr);
        exit(1);
    }
    //read arguments
    if (argc != 6){
        fputs("error: invalid arguments\n", stderr); //v pripade nevhodneho poctu argumentu
        return 1;
    }
    char *chyba = NULL;
    pi = strtol(argv[1], &chyba,10);
    ig = strtol(argv[2], &chyba,10);
    jg = strtol(argv[3], &chyba,10);
    it = strtol(argv[4], &chyba,10);
    jt = strtol(argv[5], &chyba,10);
    if (*chyba != 0){
        fputs("error: invalid arguments\n", stderr);
        return 1;
    }
    if (init() == -1){
        cleanup();
        fputs("probehl cleanup nepovedeneho initu", stderr);
        return 1;
    }
    for(int i = 0; i < pi; i++){
        (*leftovers)++;
    }
    (*rolling) = 1;
    pid_t judge = fork();
    if (judge == 0){
        processJudge();
        exit(0);
    }
    else if (judge < 0){
        fputs("judge process error", stderr);
        exit(1);
    } else {
        pid_t generator = fork();
        if (generator == 0) {
            generateImmigrants(pi, ig);
            exit(0);
        } else if (generator < 0) {
            exit(1);
        }
    }
    sem_wait(programStops);
    cleanup();
    exit(0);

}
//////////////////////////////////////FUNCTIONS/////////////////////////////////////////////////
////////////////////////////////////////judge functions:////////////////////////////////////////
void processJudge(){
    mySleep(jg);
    while(*leftovers > 0) {
        sem_wait(s_outside);
        sem_wait(mutex_checking);
        judgeEnters(); //wants to enter..............enters
        (*judgeInside) = 1;
        judgeDecides(); // waits for imm.............starts confirmation..........ends confirmation
        judgeLeaves(); //leaves
        sem_post(s_outside);
        sem_post(mutex_checking);
        (*judgeInside) = 0;
        mySleep(jg);
    }
    judgeFinishes();   // finishes
}
void judgeEnters(){
    judgePrintShort("wants to enter");
    judgePrint("enters");
}
void judgeDecides(){
    if(*ne > *nc){
        sem_post(mutex_checking);
        judgePrint("waits for imm");
        sem_wait(allRegistered);
    }
    if(*ne == *nc){
        judgePrint("starts confirmation");
        mySleep(jt);
        (*ne) = 0;
        (*nc) = 0;
        judgePrint("ends confirmation");
        sem_post(judgeHasDecided);
    }
}
void judgeLeaves(){
    mySleep(jt);
    (*conditionDecision) = 0;
    judgePrint("leaves");
}
void judgeFinishes(){
    judgePrintShort("finishes");
    (*rolling)--; //ends help process
    sem_post(programStops);
}
/////////////////////////////////////////////PRINT FUNCTIONS////////////////////////////////////
void judgePrint(char *thing){
    sem_wait(forPrint);
    (*line)++;
    fprintf(pfile,"%d   : JUDGE         : %s                  : %d    : %d    : %d \n", *line, thing, *ne, *nc, *nb);
    fflush(pfile);
    sem_post(forPrint);
}
void judgePrintShort(char *thing){
    sem_wait(forPrint);
    (*line)++;
    fprintf(pfile,"%d   : JUDGE         : %s                        \n", *line, thing);
    fflush(pfile);
    sem_post(forPrint);
}
void immPrintShort(int immNumb){
    sem_wait(forPrint);
    (*line)++;
    fprintf(pfile,"%d   : IMM %d        : starts                    \n", *line,immNumb);
    fflush(pfile);
    sem_post(forPrint);
}
void immPrint(int immNumb, char *thing){
    sem_wait(forPrint);
    (*line)++;
    fprintf(pfile,"%d   : IMM %d        : %s           : %d    : %d    : %d \n", *line, immNumb, thing, *ne, *nc, *nb);
    fflush(pfile);
    sem_post(forPrint);
}
/////////////////////////////////////////////IMMIGRANT_FUNCTIONS////////////////////////////////
void generateImmigrants(int ipi, int iig){
    for(int i = 0; i < ipi; i++) {
        pid_t IMM_ID = fork();
        immNum++;
        if (IMM_ID == 0) {
            processImmigrant();
            exit(0);
        }
        else if (IMM_ID < 0){
            fputs("generating immigrant error", stderr);
            exit(1);
        }
        mySleep(iig);
    }
}
void processImmigrant(){
    int thisImmNum = immNum;
    immPrintShort(thisImmNum);  //starts
    sem_wait(s_outside);    //ceka, pokud je soudce v budove
    sem_wait(mutex_entering);  //vstupuje po jednoum
    immigrantEnters(thisImmNum); //enters
    sem_post(s_outside);
    sem_post(mutex_entering); //uvolni vchod
    sem_wait(mutex_checking); //registruje se singl
    immChecks(thisImmNum); //checks
    if ((*judgeInside) == 1 && (*ne) == (*nc)){
        sem_post(allRegistered);
    }
    else{
        sem_post(mutex_checking); //uvolni registraci
    }
    sem_wait(judgeHasDecided);
    immWantsCertificate(thisImmNum);
    sem_wait(s_outside);
    immLeaves(thisImmNum);
    sem_post(s_outside);
}
void immigrantEnters(int immNumb){
    (*ne)++;
    (*nb)++;
    immPrint(immNumb, "enters");
}
void immChecks(int immNumb){
    (*nc)++;
    immPrint(immNumb, "checks");
}
void immWantsCertificate(int immNumb){
    immPrint(immNumb, "wants certificate");
    mySleep(it);
    immPrint(immNumb, "got certificate");
}
void immLeaves(int immNumb){
    (*nb)--;
    (*leftovers)--;
    immPrint(immNumb, "leaves");
}
//overall functions
void mySleep(int max_time){
    if (max_time != 0){
        sleep((rand()%max_time)/1000);
    }
}
int init(){
    MAKEMAP(line);
    MAKEMAP(ne);
    MAKEMAP(nc);
    MAKEMAP(nb);
    MAKEMAP(rolling);
    MAKEMAP(judgeInside);
    MAKEMAP(conditionDecision);
    MAKEMAP(leftovers);
    MAKEMAP(s_outside);
    MAKEMAP(mutex_entering);
    MAKEMAP(mutex_checking);
    MAKEMAP(programStops);
    MAKEMAP(allRegistered);
    MAKEMAP(judgeHasDecided);
    MAKEMAP(forPrint);
    MAKEMAP(immEntry);
    if ((immEntry = sem_open("xheinz01.proj2.immEntry", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) return -1;
    if ((forPrint = sem_open("xheinz01.proj2.forPrint", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) return -1;
    if ((judgeHasDecided = sem_open("xheinz01.proj2.judgeHasDecided", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) return -1;
    if ((allRegistered = sem_open("xheinz01.proj2.allRegistered", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) return -1;
    if ((programStops = sem_open("xheinz01.proj2.stops", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) return -1;
    if ((s_outside = sem_open("xheinz01.proj2.s_outside", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) return -1;
    if ((s_inside = sem_open("xheinz01.proj2.s_inside", O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) return -1;
    if ((mutex_entering = sem_open("xheinz01.proj2.mutex_entering", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) return -1;
    if ((mutex_checking = sem_open("xheinz01.proj2.mutex_checking", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) return -1;
    return 0;
}
void cleanup(){
    UNMAP(line);
    UNMAP(ne);
    UNMAP(nc);
    UNMAP(nb);
    UNMAP(rolling);
    UNMAP(judgeInside);
    UNMAP(conditionDecision);
    UNMAP(leftovers);
    UNMAP(s_outside);
    UNMAP(mutex_entering);
    UNMAP(mutex_checking);
    UNMAP(programStops);
    UNMAP(allRegistered);
    UNMAP(judgeHasDecided);
    sem_close(judgeHasDecided);
    UNMAP(forPrint);
    UNMAP(immEntry);
    sem_unlink("xheinz01.proj2.immEntry");
    sem_close(immEntry);
    sem_unlink("xheinz01.proj2.forPrint");
    sem_close(forPrint);
    sem_unlink("xheinz01.proj2.judgeHasDecided");
    sem_close(allRegistered);
    sem_unlink("xheinz01.proj2.allRegistered");
    sem_close(programStops);
    sem_unlink("xheinz01.proj2.stops");
    sem_close(s_inside);
    sem_unlink("xheinz01.proj2.s_inside");
    sem_close(s_outside);
    sem_unlink("xheinz01.proj2.s_outside");
    sem_close(mutex_entering);
    sem_unlink("xheinz01.proj2.mutex_entering");
    sem_close(mutex_checking);
    sem_unlink("xheinz01.proj2.mutex_checking");
    if (pfile != NULL){
        fclose(pfile);
    }
}

