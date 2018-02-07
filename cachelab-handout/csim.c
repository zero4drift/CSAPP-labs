#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "getopt.h"
#include "cachelab.h"

/* 当使用 ./csim -h 或错误的参数或没有参数时打印这个帮助信息 */
void printUsage(){
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options\n");
    printf("  -h        Print this help message.\n");
    printf("  -v        Optional verbose flag.\n");
    printf("  -s <num>: Number of set index bits.\n");
    printf("  -E <num>: Number of lines per set.\n");
    printf("  -b <num>: Number of block offset bits.\n");
    printf("  -t <file>: Trace file.\n");
    printf("\n");
    printf("Exampes:\n");
    printf("  linux> ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux> ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

int main(int argc, char** argv)
{
  unsigned long address;
  int s, E, b, bytes, v = 0;
  char* t;
  char input, type;
  FILE* fp;
  /* 解析命令行参数 */
  while((input=getopt(argc,argv,"s:E:b:t:vh")) != -1)
    {
      switch(input){
      case 's':
	s = atoi(optarg);
	break;
      case 'E':
	E = atoi(optarg);
	break;
      case 'b':
	b = atoi(optarg);
	break;
      case 't':
	t = optarg;
	break;
      case 'v':
	v = 1;
	break;
      case 'h':
	printUsage();
	exit(0);
      default:
	printUsage();
	exit(-1);
      }
    }
  /* 读入文件并解析 */
  fp = fopen(t, "r");
  if(fp == NULL)
    {
      printf("%s: No such file or directory\n", t);
      exit(1);
    }
  else
    {
      while(fscanf(fp, " %c %lx,%d", &type, &address, &bytes) != EOF)
	{
	  /* 下面一行只是用来测试，代码完成后删掉 */
	  printf("%c %ld,%d\n", type, address, bytes);
	}
      fclose(fp);
    }
  /* 下面一行只是用来测试，代码完成后删掉 */
  printf("s: %d E: %d b: %d v: %d t: %s\n", s, E, b, v, t);
  printSummary(0, 0, 0);
  return 0;
}
