#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

static const int  FILE_SEEK_ERROR =-2;

#define PAGES_CAP 1024
#define PAGE_SIZE 4096
#define PAGE_MASK 0xfff
static int64_t mem[PAGE_SIZE];
typedef struct MemPage MemPage;
struct MemPage{
  int64_t pageId;
  int64_t* data;
  MemPage* next;
};
static MemPage memPages[PAGES_CAP];

//XXX? unlimited stacks
static int64_t valStack[1024];//XXX? allow multiple types: int float list
static int64_t valCount=0;
static size_t valCap=1024;
static size_t ipStack[1024];
static size_t ipCount=0;
static size_t ipCap=1024;
static int64_t forCount=0;
static int64_t whileCount=0;
static int64_t procCount=0;

static bool comment=false;
static bool stringMode=false;
static bool numberMode=false;
static bool escapeMode=false;
static bool safeMode=true;

MemPage* newPage(MemPage* target,int64_t pageId){
  target->pageId=pageId;
  target->data=calloc(PAGE_SIZE,sizeof(MemPage));
	if(target->data==NULL){
    fputs("out-of memory\n",stderr);exit(1);
	}
  target->next=NULL;
  return target;
}
int64_t* getMemory(int64_t address){//XXX? only allocate on write with non-zero value
  int64_t pageId=((uint64_t)address)/PAGE_SIZE;
  if(pageId==0)
    return &mem[address];
  MemPage* page=&memPages[pageId%PAGES_CAP];
  if(page->pageId==0){
    return &newPage(page,pageId)->data[address&PAGE_MASK];
  }
	if(page->pageId==pageId)
	  return &page->data[address&PAGE_MASK];
	while(page->next!=NULL){
	  page=page->next;
	  if(page->pageId==pageId)
	    return &page->data[address&PAGE_MASK];
	}
	page->next=malloc(sizeof(MemPage));
	if(page->next==NULL){
    fputs("out-of memory\n",stderr);exit(1);
	}
	page=page->next;
  return &newPage(page,pageId)->data[address&PAGE_MASK];
}

void pushValue(int64_t val){
  if(valCount<0){
    fputs("stack-underflow\n",stderr);exit(1);
  }
  if(((size_t)valCount)>=valCap){
    fputs("stack-overflow\n",stderr);exit(1);
  }
  valStack[valCount++]=val;
}
int64_t peekValue(void){
  if(valCount<=0){
    if(safeMode){
      fputs("stack-underflow\n",stderr);exit(1);
    }
    return 0;
  }
  return valStack[valCount-1];
}
int64_t popValue(void){
  if(valCount<=0){
    if(safeMode){
      fputs("stack-underflow\n",stderr);exit(1);
      }
    return 0;
  }
  return valStack[--valCount];
}

void callStackPush(int64_t ip){
  if(ipCount>=ipCap){
    fputs("call-stack overflow\n",stderr);exit(1);
  }
  ipStack[ipCount++]=ip;
}
int64_t callStackPeek(void){
  if(ipCount<1){
    fputs("call-stack underflow\n",stderr);exit(1);
  }
  return ipStack[ipCount-1];
}
int64_t callStackPop(void){
  if(ipCount<1){
    fputs("call-stack underflow\n",stderr);exit(1);
  }
  return ipStack[--ipCount];
}

int64_t ipow(int64_t a,int64_t e){
  int64_t res=1;
	if(e<0){
		res=0;
	}else{
		while(e!=0){
			if(e&1){
				res*=a;
			}
			a*=a;
			e>>=1;
		}
	}
	return res;
}

void runProgram(char* chars,size_t size){//unused characters:  abcdefghjklmnqrstuvwxyz  #ABCDEFGHIJKLMNOPQRSTUVWXYZ
	for(size_t ip=0;ip<size;ip++){
	  if(comment){
	    if(chars[ip]=='\n')
	      comment=false;
	    continue;
	  }
		if(stringMode){
			if(escapeMode){
				escapeMode=false;
				switch(chars[ip]){
				case '"':
					pushValue('"');
					break;
				case '\\':
					pushValue('\\');
					break;
				case 'n':
					pushValue('\n');
					break;
				case 't':
					pushValue('\t');
					break;
				case 'r':
					pushValue('\r');
					break;//more escape sequences
				default:
					fprintf(stderr,"unsupported escape sequence: \\%c\n",chars[ip]);exit(1);
				}
			}else if(chars[ip]=='\\'){
				escapeMode=true;
			}else if(chars[ip]=='"'){
				stringMode=false;
				int64_t tmp=valCount-callStackPop();
				pushValue(tmp);
			}else{
				pushValue(chars[ip]);
			}
			continue;
		}
		if(procCount>0){
			if(chars[ip]=='{'){
				procCount++;
			}else if(chars[ip]=='}'){//TODO only close procedure if close on same level as open
				procCount--;
			}
			continue;
		}
		if(whileCount>0){
			if(chars[ip]=='['){
				whileCount++;
			}else if(chars[ip]==']'){
				whileCount--;
			}
			continue;
		}
		if(forCount>0){
			if(chars[ip]=='('){
				forCount++;
			}else if(chars[ip]==')'){
				forCount--;
			}
			continue;
		}
	  if(chars[ip]>='0'&&chars[ip]<='9'){
	    if(numberMode){
	      valStack[valCount-1]=10*valStack[valCount-1]+(chars[ip]-'0');
	    }else{
				pushValue(chars[ip]-'0');
	      numberMode=true;
	    }
	    continue;
	  }
	  numberMode=false;
		switch(chars[ip]){
			case '0':case '1':case '2':case '3':case '4':
			case '5':case '6':case '7':case '8':case '9'://digits have already been handled
			case ' '://ignore spaces
				break;
		  //strings&comments
			case '"':
				callStackPush(valCount);
				stringMode=true;
				break;
			case '\\':
			  comment=true;
			  break;
			//control flow
			case '[':
				if(popValue()!=0){
					callStackPush(ip);
				}else{
					whileCount=1;
				}
				break;
			case ']':
				if(popValue()!=0){
					ip=callStackPeek();
				}else{
					callStackPop();
				}
				break;
			case '(':{
			  int64_t n=popValue();
				if(n>0){
					callStackPush(ip);
					callStackPush(n);
					pushValue(n);
				}else{
					forCount=1;
				}
				}break;
			case ')':{
			  int64_t n=callStackPop();
			  n--;
				if(n>0){
					ip=callStackPeek();
					callStackPush(n);
					pushValue(n);
				}else{
					callStackPop();
				}
				}break;
			case '{':
				pushValue(ip);
				procCount=1;
				break;
			case '}':
				if(ipCount<=0){
				  if(safeMode){
					  fputs("unexpected '}'\n",stderr);exit(1);
				  }
					break;
				}
				ip=callStackPop();//return
				break;
			case '?':{
				uint64_t to=popValue();
				callStackPush(ip);
				ip=to;
				}break;
		  //stack manipulation
			case '.':;//drop   ## a ->
				popValue();
				break;
			case ':':{//dup   ## a -> a
				int64_t a=peekValue();
				pushValue(a);
				}break;
			case '\'':{//swap ## b a -> a b
				int64_t a=popValue();
				int64_t b=popValue();
				pushValue(a);
				pushValue(b);
				}break;
			case ';':{//over ## c b a -> c b a c
				int64_t a=popValue();
				int64_t c=peekValue();
				pushValue(a);
				pushValue(c);
				}break;
			case ',':{//rotate .. ## a b c -> b c a
				int64_t count=popValue();
				if(count==0)
				  break;
				if(count>0){
				  if(count>valCount){
				    if(safeMode){
					    fputs("stack underflow",stderr);exit(1);
				    }
					  pushValue(0);//? shift stack up s.t. there are count elements on stack
				    break;
				  }
				  int64_t a=valStack[valCount-count];
				  memmove(&valStack[valCount-count],&valStack[valCount-count+1],(count-1)*sizeof(*valStack));
				  valStack[valCount-1]=a;
				}else{
				  count=-count;
				  if(count>valCount){
				    if(safeMode){
					    fputs("stack underflow",stderr);exit(1);
				    }
					  popValue();//? shift stack up s.t. there are count elements on stack
				    break;
				  }
				  int64_t a=valStack[valCount-1];
				  memmove(&valStack[valCount-count+1],&valStack[valCount-count],(count-1)*sizeof(*valStack));
				  valStack[valCount-count]=a;
				}
				}break;
			//memory
			case '@':{
				int64_t addr=popValue();
				pushValue(*getMemory(addr));
				}break;
			case '$':{
				int64_t addr=popValue();
				*getMemory(addr)=popValue();
				}break;
		  //arithmetic operations
			case '+':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a+b);
				}break;
			case '-':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a-b);
				}break;
			case '*':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a*b);
				}break;
			case '/':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a/b);
				}break;
			case '%':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a%b);
				}break;
			case '>':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a>b);
				}break;
			case '<':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a<b);
				}break;
			case '=':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a==b);
				}break;
			case '&':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a&b);
				}break;
			case '|':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a|b);
				}break;
			case '^':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(a^b);
				}break;
			case '`':{
				int64_t b=popValue();
				int64_t a=popValue();
				pushValue(ipow(a,b));
				}break;
			case '!':{
				int64_t a=popValue();
				pushValue(a!=0);
				}break;
			case '~':{
				int64_t a=popValue();
				pushValue(~a);
				}break;
			case '_':{
				int64_t a=popValue();
				pushValue(-a);
				}break;
		  case 'p':{
		    int64_t v=popValue();
		    printf("%"PRIi64"\n",v);
		    }break;
		  case 'i':{
		    pushValue(getchar());
		    }break;
		  case 'o':{
		    int64_t v=popValue();
		    putchar(v&0xff);
		    }break;
			default:
				break;
		}
	}

	puts("\n------------------");
	printf("%"PRId64":",valCount);
	while(valCount>0){
		printf("%"PRId64" ",valStack[--valCount]);
	}
	puts("");
}

/* Copy from StackOverflow
 * fp is assumed to be non null
 * */
long int fsize(FILE *fp){
    long int prev=ftell(fp);
    if(prev==-1||fseek(fp, 0L, SEEK_END)!=0){
		return FILE_SEEK_ERROR;
    }
    long int sz=ftell(fp);
    //go back to where we were
    if(fseek(fp,prev,SEEK_SET)!=0){
		return FILE_SEEK_ERROR;
    }
    return sz;
}
int main(int numArgs,char** args) {
	bool loadFile;
	char *code=NULL;
	if(numArgs==2){
		loadFile=false;
		code = args[1];
	}else if(numArgs>2&&(strcmp(args[1],"-f")==0)){
		loadFile=true;
		code = args[2];
	}
	if(code==NULL){
		printf("usage: [Filename]\n or -f [Filename]");
		return EXIT_FAILURE;
	}
	long int size;
	if(loadFile){
		FILE *file = fopen(code, "r");
		if(file!=NULL){
			size=fsize(file);
			if(size==FILE_SEEK_ERROR){
				printf("IO Error while detecting file-size\n");
				return EXIT_FAILURE;
			}else if(size<0){//XXX recover form undetected file size (if seek worked)
				printf("IO Error while detecting file-size\n");
				return EXIT_FAILURE;
			}else{
				code = malloc((size+1)*sizeof(char));
				if(code==NULL){
					printf("Memory Error\n");
					return EXIT_FAILURE;
				}
				size=fread(code,sizeof(char),size,file);
				if(size<0){
					printf("IO Error while reading file\n");
					free(code);
					return EXIT_FAILURE;
				}
				fclose(file);//file no longer needed (whole contents are buffered)
			}
			if(code==NULL){
				printf("Memory Error\n");
				return EXIT_FAILURE;
			}
		}else{
			printf("IO Error while opening File: %s\n",code);
			return EXIT_FAILURE;
		}
		code[size]='\0';//null-terminate string (for printf)
	}else{
		size=strlen(code);
	}
	//print buffer
	runProgram(code,size);
	if(loadFile){
		free(code);
	}
	return EXIT_SUCCESS;
}

