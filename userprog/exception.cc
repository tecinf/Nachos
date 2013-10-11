// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "translate.h"

int opcion=0;
int tope;
static int fallos=0;
	TranslationEntry *pila;

//++++++++++++++++++++++++++++++


int band=0;
static int frUs=0;


void SustituirPagina(int tipo);
void aumentaLRU( TranslationEntry *TablaPag, unsigned int Tam);


TranslationEntry* EligeVictimaLRU(AddrSpace* space, TranslationEntry *TablaPag, unsigned int Tam);
TranslationEntry* FIFO (AddrSpace* space);
TranslationEntry* EligeVictimaReloj(AddrSpace* space);
//++++++++++++++++++++++++++++++
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    //printf("ocurrio una execpcion\n");
    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "*******Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
       else if(which == PageFaultException)
        {		
          		stats->numPageFaults++;                       
			SustituirPagina(tipo);		
        }
    else {
	printf("******Unexpected user mode exception %d %d\n", which, type);// ocurre la ecepsion
	ASSERT(FALSE);
    }
}

//Función para buscar la página que se debe cargar y la víctima que le cede el lugar.
void SustituirPagina(int tipo)
{
    unsigned int paginaVirtual;//Número de la Página que genera el fallo
    unsigned int paginaFisica;//Número del marco en el que debe cargar la página máx 32 marcos
    TranslationEntry* victima;
    TranslationEntry* nueva;
    
    //Leemos la página del registro 39 y se divide entre el tamaño
    //de páginas para saber el número de página que es.
    paginaVirtual=(machine->ReadRegister(BadVAddrReg)/PageSize);
    
    printf("Fallo página número: %d\n",paginaVirtual);

    if(frUs< NumPhysPages)	
    {
      paginaFisica = frUs;
      frUs++;
      printf("Se coloco en el marco disponible número: %d\n\n",paginaFisica);
    }
    else
    {      
     
      switch(tipo)
      {
        
		case 0:
            victima=EligeVictimaLRU(currentThread->space, machine->pageTable, machine->pageTableSize);
        break;
	case 1:
            victima = FIFO(currentThread->space);
        break;
        case 2:
            victima = EligeVictimaReloj(currentThread->space);
        break;
      }
      if (victima->dirty)
      {
         swap->WriteAt(&(machine->mainMemory[(victima->physicalPage)*PageSize]),PageSize,(victima->virtualPage)*PageSize);
         stats->numDiskWrites++;
      }

      // Marcamos dicha pagina como no valida (porque deja de estar en la memoria principal)
      victima->valid=FALSE;
      
      // obtenemos el número de marco donde estaba la victima
      paginaFisica = victima->physicalPage;
     
    }   
   
    nueva = (currentThread->space)->LeerEntrada(paginaVirtual);
    // Leemos del fichero de intercambio la pagina virtual referenciada y la traemos a memoria al lugar ocupado por la pagina sustituida
    swap->ReadAt(&(machine->mainMemory[(paginaFisica)*PageSize]),PageSize,paginaVirtual*PageSize);
    stats->numDiskReads++;
    // Actualizamos la entrada de la tabla de paginas correspondiente
    nueva->valid = TRUE;//bit de validez en true porque ya esta en la memoria
    nueva->physicalPage = paginaFisica;//num de marco a la q se asigno
}



TranslationEntry* EligeVictimaLRU(AddrSpace* space, TranslationEntry *TablaPag, unsigned int Tam)
{
    static int marco=0;					//Ultimo marco que se usa
    int entrada;					//Entrada de la tabla de paginas que corresponde a la victima
    TranslationEntry* victima;				//Victima seleccionada segun el algoritmo LRU
    int MasLejana = 0;					//Variable mas Lejana(en tiempo lest reciently used)
    int i;


	MasLejana =0;                           
	for(i=0; i<Tam&&MasLejana==0;i++)	//Buscar el marco valido 
	{
	    if(TablaPag[i].valid)			
	    {
	      MasLejana = TablaPag[i].timer;		
	      marco = TablaPag[i].physicalPage;	 
	    }
	}        

	for(i=0; i<Tam;i++) 			//Buscar la pagina mas lejana entre las  
	{            
	    if(TablaPag[i].valid ) 
	    {
		if(MasLejana > TablaPag[i].timer)  //Busco el mas lejano
		{
		    MasLejana = TablaPag[i].timer;
		    marco = TablaPag[i].physicalPage;
		}
	    }
	}

     marco=(marco)%NumPhysPages;
     entrada = space->BuscarPaginaFisica(marco);
     victima = space->LeerEntrada(entrada);
     victima->timer =0;
     printf("Se intercambio por la página número: %d \n Marco victima número: %d\n\n",victima->physicalPage,marco);       
     return victima;
}

//*****************************************************************************************
// Algoritmo de Reemplazo: FIFO
//*****************************************************************************************

TranslationEntry* FIFO(AddrSpace* space)
{  
  static int Marco=0;

  int posPag = space->BuscarPaginaFisica(Marco); //Se busca la posicion de la pagina 

  TranslationEntry* pagVictima = space->LeerEntrada(posPag); //se obtiene la pagina en sí

  Marco = (Marco+1)%NumPhysPages;

  printf("Se intercambio por la página número: %d \n Marco victima número: %d\n\n",
  pagVictima->physicalPage,Marco);

  return pagVictima; //regresa la página victima
}
//*****************************************************************************************
// Algoritmo de Reemplazo: Reloj o 2da Oportunidad
//*****************************************************************************************

TranslationEntry* EligeVictimaReloj(AddrSpace* space)
{
	static int marco=0;//Ultimo marco que se usa
	bool encontroVictima=false;
	int entrada;//Entrada de la tabla de paginas que corresponde a la victima
	TranslationEntry* victima;//Victima seleccionada

	do{
		entrada = space->BuscarPaginaFisica(marco);
		victima = space->LeerEntrada(entrada);

		if(victima!=NULL)
		{ 
			//Checar el bit de referencia
			if(victima->use==TRUE)//Si la página ha sido referenciada
			{ 
				victima->use=FALSE;//Se pone en falsa, pero se mantiene en memoria(2a oportunidad)
			}
			else//si esa página ya no ha sido referenciada, esa es la victima
			{
				encontroVictima=true;
			}
		}     
		marco=(marco+1)%NumPhysPages;//avanza al sig marco
	}while(!encontroVictima);//mientras no ncuentre una página con bit de referencia=0
        printf("Se intercambio por la página número: %d \n Marco victima número: %d\n\n",victima->physicalPage,marco-1);
	return victima;
}
