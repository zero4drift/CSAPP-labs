#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "getopt.h"
#include "time.h"
#include "cachelab.h"

typedef struct
{
  unsigned long tag;			/* 主存标记 */
  int valid;			/* 有效位 */
  clock_t time_stamp;		/* 时间戳实现LRU算法 */
}cache_line;

cache_line** initiate(int set_index_bits, int associativity){
  /* 初始化 cache_line 二维数组 cache 并分配空间 */
  int i;
  int sets = 1 << set_index_bits;
  unsigned int size;
  cache_line** cache;
  cache = (cache_line** ) malloc(sizeof(cache_line) * sets);
  for(i=0;i<sets;i++)
    {
      size = sizeof(cache_line) * associativity;
      cache[i] = (cache_line* ) malloc(size);
      /* 可以用 memset 初始化内容全为0,总比一个嵌套 for 循环好吧 */
      memset(cache[i], 0, size);
    }
  return cache;
}

int clean(cache_line** c, int set_index_bits){
  /* cache_line 二维数组的清理工作 */
  int i;
  int sets = 1 << set_index_bits;
  for(i=0;i<sets;i++)
    {
      free(c[i]);
    }
  free(c);
  return 0;
}

int hit_or_miss(cache_line* line, int line_length, unsigned long add_tag){
  /* hit: 1, miss: 0 */
  int i, result = 0;
  for(i=0;i<line_length;i++)
    {
      if((add_tag == line[i].tag) && (line[i].valid == 1))
	{
	  result = 1;
	  line[i].time_stamp = clock(); /* 如果 hit 的话,更新时间戳 */
	  break;
	}
    }
  return result;
}

int put_in_cache(cache_line* line, int line_length, unsigned long add_tag){
  /* no eviction: 0, eviction: 1 */
  int i, index = 0, result = 0;
  clock_t temp_stamp = line[0].time_stamp;
  /* 一个 trick,反正是模拟,有空位就放入并结束程序 */
  for(i=0;i<line_length;i++)
    {
      if(line[i].valid == 0)
	{
	  line[i].tag = add_tag;
	  line[i].valid = 1;
	  line[i].time_stamp = clock();
	  return result;
	}
    }
  /* 没有空位,那就比较时间戳,使用LRU算法 */
  for(i=0;i<line_length;i++)
    {
      if(temp_stamp > line[i].time_stamp)
	{
	  temp_stamp = line[i].time_stamp;
	  index = i;	  
	}
    }
  result = 1;
  line[index].tag = add_tag;
  line[index].time_stamp = clock();
  return result;
}

void print_verbose(char* pre, char type, int hit_miss, int eviction){
  /* 命令行带 -v 的话的详细数据输出函数 */
  char* h = hit_miss?" hit":" miss";
  char* e = eviction?" eviction":"";
  char* format;
  if(type == 'M')
    {
      format = "%s%s%s\n";
      strcat(pre, format);
      printf(pre, h, e, " hit");
    }
  else
    {
      format = "%s%s\n";
      strcat(pre, format);
      printf(pre, h, e);
    }
}

int cache_access(cache_line** cache_sets, int s, int E, int b, int v, int bytes, int* hits, int* misses, int* evictions, unsigned long addr, char type){
  /* 缓存模拟核心逻辑 */
  int hit_miss = 0, evictions_or_not = 0;
  char pre[20];
  /* 如果被 datalab 虐过,那下面的 << >> 应该不会陌生 */
  unsigned long tag = addr >> (b + s), sets = ((addr << (64 - b - s)) >> (64 -s));
  cache_line* set = cache_sets[sets];
  /* 尝试读取 */
  hit_miss = hit_or_miss(set, E, tag);
  /* 如果没命中的话就把这个主存块放入缓存,观察是否有 eviction */
  if(!hit_miss)
    {
      evictions_or_not = put_in_cache(set, E, tag);
    }
  /* 如果命令行带 -v 的话,打印详细信息 */
  if(v)
    {
      sprintf(pre, "%c %lx,%d", type, addr, bytes);
      print_verbose(pre, type, hit_miss, evictions_or_not);
    }
  /* 统计工作 */
  *hits += hit_miss;
  if(type=='M')
    {
      *hits += 1;
    }
  *misses += !hit_miss;
  *evictions += evictions_or_not;
  return 0;
}

/* 当使用 ./csim -h 或错误的参数或没有参数时打印这个帮助信息 */
void print_usage(){
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
  int s, E, b, bytes, has_opt = 0, hits = 0, misses = 0, evictions = 0, v = 0;
  char* t;
  char input, type;
  FILE* fp;
  cache_line** cache;
  /* 解析命令行参数 */
  while((input=getopt(argc,argv,"s:E:b:t:vh")) != -1)
    {
      has_opt = 1;
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
	print_usage();
	exit(0);
      default:
	print_usage();
	exit(-1);
      }
    }
  if(!has_opt){
        printf("./csim: Missing required command line argument\n");
        print_usage();
        return 0;
    }
  /* 根据参数初始化二维数组 */
  cache = initiate(s, E);
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
	  /* 'I' 类型的指令读取我们不关心 */
	  if(type == 'I')
	    {
	      continue;
	    }
	  else
	    {
	      /* 得到详细参数,进入缓存模拟核心逻辑 */
	      cache_access(cache, s, E, b, v, bytes, &hits, &misses, &evictions, address, type);
	    }
	}
      fclose(fp);
    }
  printSummary(hits, misses, evictions);
  /* 记得 free 掉给二维数组分配的堆空间 */
  clean(cache, s);
  return 0;
}
