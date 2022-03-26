#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "mymemory.h"

chunkStatus *head = NULL;
chunkStatus *lastVisited = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *brkPoint0 = NULL;

/* findChunk: поиск первого фрагмента, который соответствует (размер равен или больше) запросу
пользователя.
 chunkStatus *headptr: указатель на первый блок памяти в куче
unsigned int size: размер, запрошенный пользователем
retval: указатель на блок, который соответствует запросу
, или NULL, в случае, если такого блока в списке нет
*/

chunkStatus* findChunk(chunkStatus *headptr, unsigned int size)
{
  chunkStatus* ptr = headptr;
  
  while(ptr != NULL)
  {
    if(ptr->size >= (size + STRUCT_SIZE) && ptr->available == 1)
    {
      return ptr;
    }
    lastVisited = ptr;
    ptr = ptr->next;
  }  
  return ptr;  
}


/* splitChunk: разделить один большой блок на два. Первый будет иметь размер, запрошенный пользователем.
 у второго будет остаток.
 chunkStatus* ptr: указатель на блок памяти, который будет разделен.
 unsigned int size: размер, запрошенный пользователем
retval: void, функция изменяет список
*/

void splitChunk(chunkStatus* ptr, unsigned int size)
{
  chunkStatus *newChunk;	
  
  newChunk = ptr->end + size;
  newChunk->size = ptr->size - size - STRUCT_SIZE;
  newChunk->available = 1;
  newChunk->next = ptr->next;
  newChunk->prev = ptr;
  
   if((newChunk->next) != NULL)
   {      
      (newChunk->next)->prev = newChunk;
   }
  
  ptr->size = size;
  ptr->available = 0;
  ptr->next = newChunk;
}


/* inscreaseAllocation: 
  увеличьте объем доступной памяти в куче, используя ее chunkstatus точки останова
  
  *ptr: указатель на блок памяти, который будет разделен.
  unsigned int size: размер, запрошенный пользователем
  retval: void, функция изменяет список
*/
chunkStatus* increaseAllocation(chunkStatus *lastVisitedPtr, unsigned int size)
{
  brkPoint0 = sbrk(0);
  chunkStatus* curBreak = brkPoint0;		//Текущая точка останова кучи
  
  if(sbrk(MULTIPLIER * (size + STRUCT_SIZE)) == (void*) -1)
  {
    return NULL;
  }
  
  curBreak->size = (MULTIPLIER * (size + STRUCT_SIZE)) - STRUCT_SIZE;
  curBreak->available = 0;
  curBreak->next = NULL;
  curBreak->prev = lastVisitedPtr;
  lastVisitedPtr->next = curBreak;
  
  if(curBreak->size > size)
    splitChunk(curBreak, size);
  
  return curBreak;  
}


/* mergeChunkPrev: объединить один освобожденный фрагмент с его предшественником (в случае, если он также свободен)
    chunkStatusStatus* освобожден: указатель на блок памяти, который должен быть освобожден.
    retval: void, функция изменяет список
*/
void mergeChunkPrev(chunkStatus *freed)
{ 
  chunkStatus *prev;
  prev = freed->prev;
  
  if(prev != NULL && prev->available == 1)
  {
    prev->size = prev->size + freed->size + STRUCT_SIZE;
    prev->next = freed->next;
    if( (freed->next) != NULL )
      (freed->next)->prev = prev;
  }
}

/* mergeChunkNext: объединить один освобожденный фрагмент со следующим фрагментом (в случае, если он также свободен)
    chunkStatushunkStatus* освобожден: указатель на блок памяти, который должен быть освобожден.
    retval: void, функция изменяет список
*/
void mergeChunkNext(chunkStatus *freed)
{  
  chunkStatus *next;
  next = freed->next;
  
  if(next != NULL && next->available == 1)
  {
    freed->size = freed->size + STRUCT_SIZE + next->size;
    freed->next = next->next;
    if( (next->next) != NULL )
      (next->next)->prev = freed;
  }
}


/* printList: печать всего связанного списка. Для целей отладки
chunkStatus* headptr: указывает на начало списка
retval: недействительно, просто выведите
*/
void printList(chunkStatus *headptr)
{
int i = 0;
  chunkStatus *p = headptr;
  
  while(p != NULL)
  {
    printf("[%d] p: %d\n", i, p);
    printf("[%d] p->size: %d\n", i, p->size);
    printf("[%d] p->available: %d\n", i, p->available);
    printf("[%d] p->prev: %d\n", i, p->prev);
    printf("[%d] p->next: %d\n", i, p->next);
    printf("__________________________________________________\n");
    i++;
    p = p->next;
  }
}

/* mymalloc:  выделяет память в куче запрошенного размера. 
              Возвращаемый блок памяти всегда должен быть дополнен так, 
              чтобы он начинался и заканчивался на границе слова.
    
    unsigned int size: количество выделяемых байтов.
    retval: указатель на выделенный блок памяти или NULL, если память не может быть выделена.
    (ПРИМЕЧАНИЕ: система также устанавливает errno, но мы не являемся системой, поэтому 
    вы не обязаны это делать.)
*/
void *mymalloc(unsigned int _size) 
{
  //pthread_mutex_lock(&lock);
  
  
  void *brkPoint1;
  unsigned int size = ALIGN(_size);
  int memoryNeed = MULTIPLIER * (size + STRUCT_SIZE);
  chunkStatus *ptr;
  chunkStatus *newChunk;

  pthread_mutex_lock(&lock);
  brkPoint0 = sbrk(0);
  
  
  if(head == NULL)				//Создать пустой список, если в первый раз
  {
    if(sbrk(memoryNeed) == (void*) -1)		//проверка на ошибки
    {
      pthread_mutex_unlock(&lock);
      return NULL;
    }
    
   //Создайть первый блок размером, равным всей памяти, доступной в куче, после установки новой точки останова     brkPoint1 = sbrk(0);
    head = brkPoint0;
    head->size = memoryNeed - STRUCT_SIZE;
    head->available = 0;
    head->next = NULL;
    head->prev = NULL;
    
    // Разделите фрагмент на два: один с запросом размера пользователем, другой с оставшейся частью.
    ptr = head;
    
    //Проверка необходимости разделения при первом распределении
    if(MULTIPLIER > 1)  
      splitChunk(ptr, size);

    pthread_mutex_unlock(&lock);
    
    return ptr->end;
  }
  
  else//если не в первый раз запускаем								
  {
    chunkStatus *freeChunk = NULL;
    freeChunk = findChunk(head, size);
    
    if(freeChunk == NULL)					//Не нашел ни одного доступного фрагмента
    {
      freeChunk = increaseAllocation(lastVisited, size);	//Расширить кучу
      if(freeChunk == NULL) 					//Не удалось расширить кучу. увеличение распределения вернуло значение NULL (ошибка sbrk)
      {
	pthread_mutex_unlock(&lock);
	return NULL;
      }
      pthread_mutex_unlock(&lock);
      return freeChunk->end;
    }
    
    else					//Был найден фрагмент
    {
      if(freeChunk->size > size)			//Если часть памяти слишком велик, разделлить его
	splitChunk(freeChunk, size);
    }    
    pthread_mutex_unlock(&lock);    
    return freeChunk->end;
  }  
}

/* myfree: нераспределенная память, выделенная с помощью mymalloc.
void *ptr: указатель на первый байт блока памяти, выделенного
mymalloc.
retval: 0, если память была успешно освобождена, и 1 в противном случае.
(ПРИМЕЧАНИЕ: системная версия free не возвращает ошибок.)
*/
unsigned int myfree(void *ptr) {
	//#if SYSTEM_MALLOC
	/*free(ptr);
	return 0;
	#endif	
	return 1; */
	pthread_mutex_lock(&lock);
	
	chunkStatus *toFree;
	toFree = ptr - STRUCT_SIZE;
	
	if(toFree >= head && toFree <= brkPoint0)
	{
	  toFree->available = 1;	
	  mergeChunkNext(toFree);
	  mergeChunkPrev(toFree);
	  pthread_mutex_unlock(&lock);
	  return 0;
	  
	}
	else
	{
	  //#endif
	  pthread_mutex_unlock(&lock);
	  return 1;
	}
}

