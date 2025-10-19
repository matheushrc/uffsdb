#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "memoryContext.h"
#ifndef FBTREE // includes only if this flag is not defined (preventing duplication)
   #include "btree.h"
#endif
////
#ifndef FMACROS // garante que macros.h não seja reincluída
   #include "macros.h"
#endif
///
#ifndef FTYPES // garante que types.h não seja reincluída
  #include "types.h"
#endif
////
#ifndef FMISC // garante que misc.h não seja reincluída
  #include "misc.h"
#endif

#ifndef FBUFFER // garante que buffer.h não seja reincluída
  #include "buffer.h"
#endif

#ifndef FDICTIONARY // the same
  #include "dictionary.h"
#endif

#ifndef FEXPRESSAO
  #include "Expressao.h"
#endif
/* ----------------------------------------------------------------------------------------------
    Objetivo:   Recebe o nome de uma tabela e engloba as funções leObjeto() e leSchema().
    Parametros: Nome da Tabela, Objeto da Tabela e tabela.
    Retorno:    tp_table
   ---------------------------------------------------------------------------------------------*/
tp_table *abreTabela(char *nomeTabela, struct fs_objects *objeto, tp_table **tabela) {
    *objeto     = leObjeto(nomeTabela);
    *tabela     = leSchema(*objeto);

    return *tabela;
}
// Se foram especificadas colunas no *s_insert, verifica se elas existem no esquema.
int allColumnsExists(rc_insert *s_insert, table *tabela) {
	int i;
	if (!s_insert->columnName) return 0;

	for (i = 0; i < s_insert->N; i++)
		if (retornaTamanhoTipoDoCampo(s_insert->columnName[i], tabela) == 0) {
      if (!verificaNomeTabela(tabela->nome)) {
        printf("ERROR: table \"%s\" does not exist.\n", tabela->nome);
        return 0;
      }
			printf("ERROR: column \"%s\" of relation \"%s\" does not exist.\n", s_insert->columnName[i], tabela->nome);
			return 0;
		}

	return 1;
}
////
int typesCompatible(char table_type, char insert_type) {
	return (table_type == 'D' && insert_type == 'I')
		|| (table_type == 'D' && insert_type == 'D')
		|| (table_type == 'I' && insert_type == 'I')
		|| (table_type == 'S' && insert_type == 'S')
		|| (table_type == 'S' && insert_type == 'C')
		|| (table_type == 'C' && insert_type == 'C')
		|| (table_type == 'C' && insert_type == 'S');
}
////
// Busca o tipo do valor na inserção *s_insert do valor que irá para *columnName
// Se não existe em *s_insert, assume o tipo do esquema já que não receberá nada.
char getInsertedType(rc_insert *s_insert, char *columnName, table *tabela) {
	int i;
	for (i = 0; i < s_insert->N; i++)
		if (objcmp(s_insert->columnName[i], columnName) == 0)
			return s_insert->type[i];
	return retornaTamanhoTipoDoCampo(columnName, tabela);;
}

int getMaxPrimaryKey(char *nomeTabela) {
    struct fs_objects objeto;
    tp_table *esquema;
    tp_buffer *bufferpoll;
    tupla *pagina;

    if (!verificaNomeTabela(nomeTabela)) {
        printf("ERROR: relation \"%s\" was not found.\n", nomeTabela);
        return ERRO_NOME_TABELA;
    }

    objeto = leObjeto(nomeTabela);
    esquema = leSchema(objeto);

    if (esquema == ERRO_ABRIR_ESQUEMA) {
        printf("ERROR: schema cannot be created.\n");
        return -1;
    } 

    bufferpoll = initbuffer();

    //TODO: Need to improve this validation, is necessary handle properly the error
    if (bufferpoll == ERRO_DE_ALOCACAO) {
        printf("ERROR: no memory available to allocate buffer.\n");
        return -1;
    }

    int erro = SUCCESS, x;
    for (x = 0; erro == SUCCESS; x++) {
        erro = colocaTuplaBuffer(bufferpoll, x, esquema, objeto);
    }

    int maiorPK = -1; 
    for (int page = 0; page < x; page++) {
        pagina = getPage(bufferpoll, esquema, objeto, page);
        if (!pagina) continue;

        for (int i = 0; i < bufferpoll[page].nrec; i++) {
            for (int j = 0; j < objeto.qtdCampos; j++) {
                column *campo = &pagina[i].column[j];
                if (campo[j].tipoCampo == 'I' && esquema[j].chave == PK) {
                    int valorPK = *((int *)campo[j].valorCampo);
                    if (valorPK > maiorPK) {
                        maiorPK = valorPK;
                    }
                }
            }
        }
    }

    if (maiorPK == -1) {
        // Se nenhuma PK foi encontrada, retorna 0 para começar com a primeira chave.
        return 0;
    }

    return maiorPK;
}

int verifyFK(char *tableName, char *column){
  int r = 0;
  if(verificaNomeTabela(tableName) == 1){
    struct fs_objects objeto = leObjeto(tableName);
    tp_table *esquema = leSchema(objeto),*k;
    if(esquema == ERRO_ABRIR_ESQUEMA){
      printf("ERROR: cannot create schema.\n");
      return 0;
    }
    for(k = esquema; k && !r; k = k->next)
      r = (k->chave == PK && objcmp(k->nome, column) == 0);
  }
  return r;
}


// Busca o valor na inserção *s_insert designado à *columnName.
// Se não existe, retorna 0, 0.0 ou \0
char *getInsertedValue(rc_insert *s_insert, char *columnName, table *tabela) {
	int i;
	for (i = 0; i < s_insert->N; i++)
		if (objcmp(s_insert->columnName[i], columnName) == 0)
			return s_insert->values[i];

  int maxPK = getMaxPrimaryKey(tabela->nome);
	char tipo = retornaTamanhoTipoDoCampo(columnName, tabela);
	char *noValue = (char *)uffslloc(50); 

	if (tipo == 'I') {
        verifyFK(tabela->nome, columnName) ? sprintf(noValue, "%d", maxPK + 1) : sprintf(noValue, "0");
	} else if (tipo == 'D') {
		sprintf(noValue, "0.0");
	} else {
		noValue[0] = '\0';
	}

	return noValue;
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Inicializa os atributos necessários para a verificação de FK e PK.
    Parametros: Objeto da tabela, Tabela, Buffer e nome da tabela.
    Retorno:    INT
                SUCCESS,
                ERRO_DE_PARAMETRO,
   ---------------------------------------------------------------------------------------------*/

int iniciaAtributos(struct fs_objects *objeto, tp_table **tabela, tp_buffer **bufferpool, char *nomeT){
    *objeto     = leObjeto(nomeT);
    *tabela     = leSchema(*objeto);
    *bufferpool = initbuffer();
    if(*tabela == ERRO_ABRIR_ESQUEMA) return ERRO_DE_PARAMETRO;
    if(*bufferpool == ERRO_DE_ALOCACAO) return ERRO_DE_PARAMETRO;
    return SUCCESS;
}
////


////////
/* ----------------------------------------------------------------------------------------------
    Objetivo:   Gera as verificações em relação a chave FK.
    Parametros: Nome da Tabela, Coluna C, Nome do Campo, Valor do Campo, Tabela Apontada e Atributo Apontado.
    Retorno:    INT
                SUCCESS,
                ERRO_DE_PARAMETRO,
                ERRO_CHAVE_ESTRANGEIRA
   ---------------------------------------------------------------------------------------------*/

int verificaChaveFK(char *nomeTabela,column *c, char *nomeCampo, char *valorCampo, char *tabelaApt, char *attApt){
    int x, erro, page;
    char str[20];
    char dat[5] = ".dat";
    struct fs_objects objeto;
    tp_table *tabela;
    tp_buffer *bufferpoll;
    tupla *pagina = NULL;

    strcpylower(str, tabelaApt);
    strcat(str, dat);              //Concatena e junta o nome com .dat

    erro = existeAtributo(nomeTabela, c);

    if(iniciaAtributos(&objeto, &tabela, &bufferpoll, tabelaApt) != SUCCESS) {
        return ERRO_DE_PARAMETRO;
    }

    erro = SUCCESS;
    for(x = 0; erro == SUCCESS; x++)
        erro = colocaTuplaBuffer(bufferpoll, x, tabela, objeto);

    for (page = 0; page < PAGES; page++) {
        pagina = getPage(bufferpoll, tabela, objeto, page);
        if (!pagina) break;
        /*
        * Pq ele percorre todas as tuplas para verificar ??????
        * o campo vai mudar de nome no select ??? ?
        * alguém deveria arrumar isso...
        */
        for(int j = 0; j < bufferpoll[page].nrec; j++){
            for (int i = 0; i < objeto.qtdCampos; i++)
                if (pagina) { // Não necessita verificar se pagina[j].column[i].nomeCampo é NULL, pois se pagina foi alocada o nomeCampo não será NULL.
                    column *c = &pagina[j].column[i];
                    if(objcmp(c->nomeCampo, attApt) == 0){

                        if(c->tipoCampo == 'S'){
                            if(objcmp(c->valorCampo, valorCampo) == 0){
                                return SUCCESS;
                            }
                        }
                        else if(c->tipoCampo == 'I'){
                            int *n = (int *)&c->valorCampo[0];
                            if(*n == atoi(valorCampo)){
                                return SUCCESS;
                            }
                        }
                        else if(c->tipoCampo == 'D'){
                            double *nn = (double *)&c->valorCampo[0];

                            if(*nn == atof(valorCampo)){
                                return SUCCESS;
                            }
                        }
                        else if(c->tipoCampo == 'C'){
                            if(c->valorCampo == valorCampo){
                                return SUCCESS;
                            }
                        }
                        else {
                            return ERRO_CHAVE_ESTRANGEIRA;
                        }
                    }
                }
        }
    }
    return ERRO_CHAVE_ESTRANGEIRA;
}
/* ----------------------------------------------------------------------------------------------
    Objetivo:   Gera as verificações em relação a chave PK.
    Parametros: Nome da Tabela, Coluna C, Nome do Campo, Valor do Campo
    Retorno:    INT
                SUCCESS,
                ERRO_DE_PARAMETRO,
                ERRO_CHAVE_PRIMARIA
   ---------------------------------------------------------------------------------------------*/
int verificaChavePK(char *nomeTabela, column *c, char *nomeCampo, char *valorCampo) {
    int j, x, erro, page;
    tupla *pagina = NULL;

    struct fs_objects objeto;
    tp_table *tabela;
    tp_buffer *bufferpoll;

    erro = existeAtributo(nomeTabela, c);
    if (erro != SUCCESS ) {
        return ERRO_DE_PARAMETRO;
    }

    if (iniciaAtributos(&objeto, &tabela, &bufferpoll, nomeTabela) != SUCCESS) {
        return ERRO_DE_PARAMETRO;
    }

    erro = SUCCESS;
    for(x = 0; erro == SUCCESS || erro == ERRO_LEITURA_DADOS_DELETADOS; x++)
        erro = colocaTuplaBuffer(bufferpoll, x, tabela, objeto);

    page = 0;
    for (page = 0; page < PAGES; page++) {
        pagina = getPage(bufferpoll, tabela, objeto, page);
        if (!pagina) break;

        for(j = 0; j < bufferpoll[page].nrec; j++){
            for(int i = 0; i < objeto.qtdCampos; i++){
                column *c = &pagina[j].column[i];
                if (objcmp(c->nomeCampo, nomeCampo) == 0) {
                    if (c->tipoCampo == 'S') {
                        if (objcmp(c->valorCampo, valorCampo) == 0){
                            return ERRO_CHAVE_PRIMARIA;
                        }
                    } else if (c->tipoCampo == 'I') {
                        int *n = (int *)&c->valorCampo[0];

                        if (*n == atoi(valorCampo)) {
                            return ERRO_CHAVE_PRIMARIA;
                        }
                    } else if (c->tipoCampo == 'D'){
                        double *nn = (double *)&c->valorCampo[0];

                        if (*nn == atof(valorCampo)){
                            return ERRO_CHAVE_PRIMARIA;
                        }
                    } else if (c->tipoCampo == 'C'){
                        if (c->valorCampo == valorCampo){
                            return ERRO_CHAVE_PRIMARIA;
                        }
                    }
                }
            }
        }
    }

    return SUCCESS;
}

/////
int finalizaInsert(char *nome, column *c, int tamTupla){
    column *auxC, *temp;
    int i = 0, x = 0, t, erro, encontrou, j = 0, flag=0;
    FILE *dados;
    nodo *raiz = NULL;
    nodo *raizfk = NULL;

    struct fs_objects objeto,dicio; // Le dicionario
    tp_table *auxT ; // Le esquema
    auxT = abreTabela(nome, &dicio, &auxT);

    table *tab     = (table *)uffslloc(sizeof(table));
    tp_table *tab2 = (tp_table *)uffslloc(sizeof(struct tp_table));
    memset(tab2, 0, sizeof(tp_table));

    tab->esquema = abreTabela(nome, &objeto, &tab->esquema);
    tab2 = procuraAtributoFK(objeto);

    //-----------------------
    char *arquivoIndice = NULL;
    //------------------------

    for(j = 0, temp = c; j < objeto.qtdCampos && temp != NULL; j++, temp = temp->next){
        switch(tab2[j].chave){
            case NPK:
                erro = SUCCESS;
            break;

            case PK:
        		if(flag == 1) break;
                //monta o nome do arquivo de indice
                if(temp->valorCampo == COLUNA_NULL) {
                    printf("ERROR: attempt to insert NULL value into collumn \"%s\".\n\n", temp->nomeCampo);
                    return ERRO_INDEX_NULL;
                }

                arquivoIndice = (char *)uffslloc(sizeof(char) *
                  (strlen(connected.db_directory) + strlen(nome) + strlen(tab2[j].nome)));
                strcpy(arquivoIndice, connected.db_directory); //diretorio
                strcat(arquivoIndice, nome); //nome da tabela
        				strcat(arquivoIndice, tab2[j].nome); //nome do atributo

        		// verificacao da chave primaria
        		raiz = constroi_bplus(arquivoIndice);
        		if(raiz != NULL) {
        			encontrou = buscaChaveBtree(raiz, temp->valorCampo); 
        			if (encontrou) {
                //Compara para ver se é not null
                if (tab2[j].chave == PK || tab2[j].chave == FK) {
                  if(strcmp(temp->valorCampo, "0") == 0){
                    printf("ERROR: NULL value in column '%s' violates NOT-NULL constraint.\n", temp->nomeCampo);
                    return ERRO_NAO_INSERIR_EM_NOT_NULL;
                  }
                }
        				printf("ERROR: duplicated key value violates unique constraint \"%s_pkey\"\nDETAIL:  Key (%s)=(%s) already exists.\n",nome,temp->nomeCampo,temp->valorCampo);
        				return ERRO_CHAVE_PRIMARIA;
        			}
        		}
        		flag = 1;
            break;

            case FK:
                if(temp->valorCampo == COLUNA_NULL) {
                    printf("ERROR: attempt to insert NULL value into collumn \"%s\".\n\n", temp->nomeCampo);
                    return ERRO_INDEX_NULL;
                }
              //monta o nome do arquivo de indice da chave estrangeira
                arquivoIndice = (char *)uffslloc(sizeof(char) *
                    (strlen(connected.db_directory) + strlen(tab2[j].tabelaApt) + strlen(tab2[j].attApt)));// caminho diretorio de arquivo de indice
                strcpy(arquivoIndice, connected.db_directory); //diretorio
                strcat(arquivoIndice, tab2[j].tabelaApt);
                strcat(arquivoIndice, tab2[j].attApt);

                raizfk = constroi_bplus(arquivoIndice); //verifica se o atributo referenciado pela FK possui indice B+
                if(raizfk == NULL) { //se não encontra faz a verificação sem indice b+
        			if (strlen(tab2[j].attApt) != 0 && strlen(tab2[j].tabelaApt) != 0){
        				erro = verificaChaveFK(nome, temp, tab2[j].nome, temp->valorCampo,
                        tab2[j].tabelaApt, tab2[j].attApt);
                        if (erro != SUCCESS){
                            printf("ERROR: invalid reference to \"%s.%s\". The value \"%s\" does not exist.\n", tab2[j].tabelaApt,tab2[j].attApt,temp->valorCampo);
                            return ERRO_CHAVE_ESTRANGEIRA;
                        }
                    }
                } else { //atributo referenciado possui indice B+
                    encontrou = buscaChaveBtree(raizfk, temp->valorCampo);
                    if (!encontrou) {
                        printf("ERROR: invalid reference to \"%s.%s\". The value \"%s\" does not exist.\n", tab2[j].tabelaApt,tab2[j].attApt,temp->valorCampo);
                        return ERRO_CHAVE_ESTRANGEIRA;
                    }
                    erro = SUCCESS;
                }
            break;
        }
    }
    flag = 0;
    char directory[LEN_DB_NAME_IO];
    strcpy(directory, connected.db_directory);
    strcat(directory, dicio.nArquivo);

    if((dados = fopen(directory,"a+b")) == NULL){
        printf("ERROR: cannot open file.\n");
        return ERRO_ABRIR_ARQUIVO;
	}
    long int offset = ftell(dados);

    fputc(0, dados); // flag para tupla não deletada

    char* buffer = (char *)uffslloc(tamTupla* sizeof(char));
    int offsetBuffer = 0;

    for(auxC = c, t = 0; auxC != NULL; auxC = auxC->next, t++){
        if (t >= dicio.qtdCampos) t = 0;

        if (auxT[t].chave == PK && flag == 0) {
			char * nomeAtrib;
      		nomeAtrib = (char*)uffslloc((strlen(nome)+strlen(auxC->nomeCampo) + strlen(connected.db_directory))* sizeof(char));
      		strcpy(nomeAtrib, connected.db_directory);
      		strcat(nomeAtrib, nome);
      		strcat(nomeAtrib,auxC->nomeCampo);
            insere_indice(raiz, auxC->valorCampo, nomeAtrib, offset);
            flag = 1;
        }

		if (auxT[t].chave == BT) {
            if(auxC->valorCampo == COLUNA_NULL) {
                printf("ERROR: attempt to insert NULL value into collumn \"%s\".\n\n", auxC->nomeCampo);
                erro = ERRO_INDEX_NULL;
                goto fim;
            }
			char * nomeAtrib2;
            //ntuplas = ntuplas-1;
            decnTuplas();
      		nomeAtrib2 = (char*)uffslloc((strlen(nome)+strlen(auxC->nomeCampo) + strlen(connected.db_directory))* sizeof(char));
      		strcpy(nomeAtrib2, connected.db_directory);
      		strcat(nomeAtrib2, nome);
      		strcat(nomeAtrib2,auxC->nomeCampo);
      		nodo * raiz2 = NULL;
      		raiz2 = constroi_bplus(nomeAtrib2);
            insere_indice(raiz2, auxC->valorCampo, nomeAtrib2, offset);
        }

        if(auxC->valorCampo == COLUNA_NULL) {
            fputc(1, dados);
            auxC->valorCampo = (char *)uffslloc(2);
    
            auxC->valorCampo[0] = '0';
            auxC->valorCampo[1] = '\0';
        } else {
            fputc(0, dados);
        }

        if (auxT[t].tipo == 'S'){ // Grava um dado do tipo string.
            if (sizeof(auxC->valorCampo) > auxT[t].tam && sizeof(auxC->valorCampo) != 8){
                printf("ERROR: invalid string length.\n");
                erro = ERRO_NO_TAMANHO_STRING;
                goto fim;
            }

            if(objcmp(auxC->nomeCampo, auxT[t].nome) != 0){
                printf("ERROR: column name \"%s\" is not valid.\n", auxC->nomeCampo);    
                erro = ERRO_NOME_CAMPO;
                goto fim;
            }

            char valorCampo[auxT[t].tam];
            strncpy(valorCampo, auxC->valorCampo, auxT[t].tam);
            //strcat(valorCampo, "\0");
             valorCampo[auxT[t].tam -1 ] = 0;
            memcpy(buffer + offsetBuffer, valorCampo, auxT[t].tam);
            offsetBuffer += auxT[t].tam;
        }
        else if (auxT[t].tipo == 'I'){ // Grava um dado do tipo inteiro.
          i = 0;
          while (i < strlen(auxC->valorCampo)){
              if((auxC->valorCampo[i] < 48 || auxC->valorCampo[i] > 57) && auxC->valorCampo[i] != 45){
                  printf("ERROR: column \"%s\" expectet integer.\n", auxC->nomeCampo);

            fclose(dados);
                  return ERRO_NO_TIPO_INTEIRO;
              }
          i++;
          }
          int valorInteiro = 0;
          sscanf(auxC->valorCampo,"%d",&valorInteiro);
          memcpy(buffer + offsetBuffer, &valorInteiro,sizeof(valorInteiro));
          offsetBuffer += sizeof(valorInteiro);
          DEBUG_PRINT("INSERT - Integer value written in file: %d", valorInteiro);
        }
        else if (auxT[t].tipo == 'D'){ // Grava um dado do tipo double.
          x = 0;
          while (x < strlen(auxC->valorCampo)){
              if((auxC->valorCampo[x] < 48 || auxC->valorCampo[x] > 57) && (auxC->valorCampo[x] != 46) && (auxC->valorCampo[x] != 45)){
                  printf("ERROR: column \"%s\" expect double.\n", auxC->nomeCampo);
                  erro = ERRO_NO_TIPO_DOUBLE;
                  goto fim;
              }
              x++;
          }
          char *endptr;
          double valorDouble = strtod(auxC->valorCampo, &endptr);


          memcpy(buffer + offsetBuffer, &valorDouble, sizeof(double));
          offsetBuffer += sizeof(valorDouble);
        }
        else if (auxT[t].tipo == 'C'){ // Grava um dado do tipo char.

            if (strlen(auxC->valorCampo) > (sizeof(char))) {
                printf("ERROR: column \"%s\" expect char.\n", auxC->nomeCampo);
                erro = ERRO_NO_TIPO_CHAR;
                goto fim;
            }
            char valorChar = auxC->valorCampo[0];
            memcpy(buffer + offsetBuffer, &valorChar, sizeof(char));
            offsetBuffer += sizeof(valorChar);

        }

    }
    erro = SUCCESS;
    fwrite(buffer, tamTupla, 1, dados);
    DEBUG_PRINT("INSERT - Tuple size written in file: %d", tamTupla);
    
    fim: //label para liberar a memória utilizada e fechar o arquivo de dados
        fclose(dados);
    return erro;
}

/* insert: Recebe uma estrutura rc_insert e valida os tokens encontrados pela interface().
 *         Se os valores forem válidos, insere um novo valor.
 */
void insert(rc_insert *s_insert) {
	int i;
	table *tabela = (table *)uffslloc(sizeof(table));
	tabela->esquema = NULL;
	memset(tabela, 0, sizeof(table));
	column *colunas = NULL;
	tp_table *esquema = NULL;
	struct fs_objects objeto;
	memset(&objeto, 0, sizeof(struct fs_objects));
	char  flag=0;

	abreTabela(s_insert->objName, &objeto, &tabela->esquema); //retorna o esquema para a insere valor
	strcpylower(tabela->nome, s_insert->objName);

   DEBUG_PRINT("INSERT - TableName <--------- %s", tabela->nome);

	if(s_insert->columnName != NULL){
		if (allColumnsExists(s_insert, tabela)){
			for (esquema = tabela->esquema; esquema != NULL; esquema = esquema->next){
				if(typesCompatible(esquema->tipo,getInsertedType(s_insert, esquema->nome, tabela))){
					colunas = insereValor(tabela, colunas, esquema->nome, getInsertedValue(s_insert, esquema->nome, tabela));
				}
        else {
					printf("ERROR: data type invalid to column '%s' of relation '%s' (expected: %c, received: %c).\n", esquema->nome, tabela->nome, esquema->tipo, getInsertedType(s_insert, esquema->nome, tabela));
					flag=1;
				}
			}
		}
    else {
			flag = 1;
		}
	}
  else {
		if (s_insert->N == objeto.qtdCampos) {
			for(i=0; i < objeto.qtdCampos; i++) {

				if(s_insert->type[i] == 'S' && tabela->esquema[i].tipo == 'C') {
					s_insert->values[i][1] = '\0';
					s_insert->type[i] = 'C';
				}

				if(s_insert->type[i] == 'I' && tabela->esquema[i].tipo == 'D') {
					s_insert->type[i] = 'D';
				}

				if(s_insert->type[i] == tabela->esquema[i].tipo)
					colunas = insereValor(tabela, colunas, tabela->esquema[i].nome, s_insert->values[i]);
				else {
					printf("ERROR: data type invalid to column '%s' of relation '%s' (expected: %c, received: %c).\n", tabela->esquema[i].nome, tabela->nome, tabela->esquema[i].tipo, s_insert->type[i]);
					flag=1;
				}
			}
		}
        else {
        printf("ERROR: INSERT has more expressions than target columns.\n");
            flag = 1;
            }
        }

    if (!flag && finalizaInsert(s_insert->objName, colunas, tamTuplaSemByteControle(tabela->esquema, objeto)) == SUCCESS)  printf("INSERT 0 1\n");

	//freeTp_table(&esquema, objeto.qtdCampos);
	freeColumn(colunas);
	freeTable(tabela);
}

//select * from t4;
int validaProj(Lista *proj, tp_table *colunas, int qtdColunas, int *indiceProj){
    if(proj->tam == 1 && ((char *)proj->prim->inf)[0] == '*'){
        rmvNodoPtr(proj, proj->prim);
        proj->prim = proj->ult = NULL;
        for(int j = 0; j < qtdColunas; j++){
            indiceProj[j] = (char) j;
            char *str = uffslloc(sizeof(char) * strlen(colunas[j].nome));
            strcpy(str, colunas[j].nome);
            adcNodo(proj, proj->ult, str);
        }
        return 1;
    }

    char *validar = uffslloc(sizeof(char) * proj->tam);
    memset(validar, 0, proj->tam); // Inicializa o vetor de validação com 0

    int i = 0;
    for(Nodo *it = proj->prim; it; it = it->prox, i++){    
        for(int j = 0; j < qtdColunas; j++){
            if (strcmp((char *)it->inf, colunas[j].nome) == 0){
                validar[i] = 1;   
                indiceProj[i] = (char) j;
            }
        }
    }
    i = 0;
    for(Nodo *it = proj->prim; it; it = it->prox, i++){
        if(!validar[i]){
            printf("A coluna da projecao %s não pertence a tabela.\n",(char *)it->inf);
            return 0;
        }
    }
    return 1;
}

inf_where *novoTokenWhere(char *str,int id){
  inf_where *novo = uffslloc(sizeof(inf_where));
  novo->id = id;
  char *tk = uffslloc(sizeof(char)*strlen(str));
  strcpy(tk,str);
  novo->token = (void *)tk;
  return novo;
}

inf_where *novoResWhere(void *tk,int id){
  if(id == STRING || id == ABRE_PARENT || id == FECHA_PARENT)
    return novoTokenWhere((char *)tk,id);
  inf_where *novo = uffslloc(sizeof(inf_where));
  novo->id = id;
  if(id == NULLA) novo->token = COLUNA_NULL;
  else if(novo->id == VALUE_NUMBER){
    double *tok = uffslloc(sizeof(double));
    *tok = *((double *)tk);
    novo->token = (void *)tok;
  }
  else{//BOOLEANO
    char *tok = uffslloc(sizeof(char));
    *tok = *((char *)tk);
    novo->token = (void *)tok;
  }
  return novo;
}

int validaColsWhere(Lista *tok,tp_table *colunas,int qtdColunas){
  if(!tok) return 1;
  for(Nodo *i = tok->prim; i; i = i->prox){
    inf_where *iw = (inf_where *)i->inf;
    if(iw->id == OBJETO){
      int achou = 0;
      char *str = (char *)iw->token;
      for(int j = 0; !achou && j < qtdColunas; j++)
        achou = (strcmp(str,colunas[j].nome) == 0);
      if(!achou){
        printf("A coluna %s não pertene a tabela.\n",str);
        return 0;
      }
    }
  }
  return 1;
}

void printConsulta(Lista *proj, Lista *result){
    if(!result->tam){
        printf("\n 0 Rows.\n");
        return;
    }
    //cabecalho
    for(Nodo *j = ((Lista *)(result->prim->inf))->prim, *i = proj->prim; j; j = j->prox, i = i->prox){
        inf_where *jw = (inf_where *)(j->inf);
        
        if(jw->id == (int)'S') printf(" %-20s ", (char *)(i->inf));
        else printf(" %-10s ", (char *)(i->inf));
        
        if(j->prox) printf("|");
    }
    printf("\n");
    for(Nodo *j = ((Lista *)(result->prim->inf))->prim; j; j = j->prox){
        inf_where *jw = (inf_where *)(j->inf);
        printf("%s",(jw->id == (int)'S') ? "----------------------" : "------------");
        if(j->prox) printf("+");
    }
    printf("\n");//fim do cabecalho
    for(Nodo *i = result->prim; i; i = i->prox){
        Lista *li = (Lista *)(i->inf);

        for(Nodo *j = li->prim; j; j = j->prox){
            inf_where *ij = (inf_where *)(j->inf);

            if(ij->token == COLUNA_NULL) {
                if(ij->id == 'S') printf(" %-20s ", "NULL");
                else  printf(" %-10s ", "NULL");
            }
            else if(ij->id == 'S')
                printf(" %-20s ", (char *)ij->token);
            else if(ij->id == 'I'){
                int *n = (int *)(ij->token);
                printf(" %-10d ", *n);
            }
            else if(ij->id == 'C') 
                printf(" %-10c ", *(char *)ij->token);
            else if(ij->id == 'D'){
                double *n = (double *)(ij->token);
                printf(" %-10f ", *n);
            }
            if(j->prox) printf("|");
        }
        printf("\n");
    }
    printf("\n %d Linha%s.\n", result->tam, result->tam == 1 ? "" : "s");
}

void adcResultado(Lista *resultado, tupla *tuple, int *indiceProj, int qtdColunasProj){
    adcNodo(resultado, resultado->ult, (void *)novaLista(NULL));
    Lista *tuplaRes = (Lista *)(resultado->ult->inf);
    inf_where **listNw = (inf_where **)uffslloc(sizeof(inf_where *) * tuple->ncols);

    for(uint i = 0; i < tuple->ncols; i++){
        inf_where *nw = uffslloc(sizeof(inf_where));
        column *c = &tuple->column[i];
        nw->id = c->tipoCampo;

        if(c->valorCampo == (void *)COLUNA_NULL) nw->token = COLUNA_NULL;
        else if(c->tipoCampo == 'S'){
            char *str = uffslloc(sizeof(char)*strlen(c->valorCampo));
            str[0] = '\0';
            strcpy(str, c->valorCampo);
            nw->token = (void *)str;
        }
        else if(c->tipoCampo == 'I'){
            int *n = uffslloc(sizeof(int));
            *n = *(int *)(c->valorCampo);
            nw->token = (void *)n;
        }
        else if(c->tipoCampo == 'C'){
            char *n = uffslloc(sizeof(char));
            *n = *(char *)(c->valorCampo);
            nw->token = (void *)n;
        }
        else if(c->tipoCampo == 'D'){
            double *n = uffslloc(sizeof(double));
            *n = *(double *)(c->valorCampo);
            nw->token = (void *)n;
        }
        listNw[i] = nw;
    }
    
    for(int i = 0; i < qtdColunasProj; i++) adcNodo(tuplaRes, tuplaRes->ult, listNw[indiceProj[i]]);

}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Utilizada para deletar tuplas.
    Parametros: Nome da tabela (char).
    Retorno:    Void.
   ---------------------------------------------------------------------------------------------*/
void op_delete(Lista *toDeleteTuples, char *tabelaName) {
    tp_table *esquema;
    struct fs_objects objeto = leObjeto(tabelaName);
    esquema = leSchema(objeto);
    tp_buffer *bufferpoll = initbuffer();
    int countDeletedTuples = 0;

    int tuplaCount = 0, erro;
    do {
        erro = colocaTuplaBuffer(bufferpoll, tuplaCount, esquema, objeto);
        tuplaCount++;
    } while(erro == SUCCESS || erro == ERRO_LEITURA_DADOS_DELETADOS);
    tuplaCount--; // ajusta para o número correto de páginas lidas

    for (Nodo *temp = toDeleteTuples->prim; temp; temp = temp->prox) {
        tupla *t = (tupla *)temp->inf;
        *(bufferpoll[t->bufferPage].data+t->offset) = 1; //marca a tupla como deletada
        bufferpoll[t->bufferPage].db = 1; //marca a página como modificada
        countDeletedTuples++;
    }

    for (int p = 0; p < PAGES && bufferpoll[p].nrec; p++) {
        int result = writeBufferToDisk(bufferpoll, &objeto, p, bufferpoll->nrec*tamTupla(esquema, objeto));
        if (!result) {
            fprintf(stderr, "ERROR: failed to persist changes to disk\n");

            return;
        }
    }

    printf("DELETED %d %s\n", countDeletedTuples, (countDeletedTuples != 1) ? "rows" : "row");


}

int afterTrigger(Lista *resultado, inf_query *query) {
    tp_table *fkColumns = verificaIntegridade(query->tabela);
    for(tp_table *temp = fkColumns; temp; temp = temp->next) {
        nodo *bplusRoot = buildBplusForPK(temp);
        for(Nodo *aux; aux; aux = aux->prox) {
            tupla *t = aux->inf;
            for(column *col = t->column; col; col = col->next){
                if(!strncmp(col->nomeCampo, temp->nome, TAMANHO_NOME_CAMPO)){
                    if(buscaChaveBtree(bplusRoot, col->valorCampo)){
                        printf("\nERROR: tuple with primary key '%s' is referenced by table '%s' via foreign key '%s'", col->nomeCampo, temp->tabelaApt, temp->nome);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

Lista *handleTableOperation(inf_query *query, char tipo) {
    tp_table *esquema;
    tp_buffer *bufferpoll;
    struct fs_objects objeto;
    if(!verificaNomeTabela(query->tabela)){
        printf("\nERROR: relation \"%s\" was not found.\n\n\n", query->tabela);
        return NULL;
    }
    objeto = leObjeto(query->tabela);
    esquema = leSchema(objeto);
    if(esquema == ERRO_ABRIR_ESQUEMA){
        printf("ERROR: schema cannot be created.\n");
        return NULL;
    }
    bufferpoll = initbuffer();
    if(bufferpoll == ERRO_DE_ALOCACAO){
        printf("ERROR: no memory available to allocate buffer.\n");
        return NULL;
    }

    int pageCount = 0, erro;
    do {
        erro = colocaTuplaBuffer(bufferpoll, pageCount, esquema, objeto);
        pageCount++;
    } while(erro == SUCCESS || erro == ERRO_LEITURA_DADOS_DELETADOS);
    pageCount--; // ajusta para o número correto de páginas lidas

    int *indiceProj = NULL, qtdCamposProj = 0;
    if(tipo == 's') {
        indiceProj = (int *)uffslloc(sizeof(int) * query->proj->tam);
        if(!validaProj(query->proj, esquema, objeto.qtdCampos, indiceProj)){
            return NULL;
        }
        qtdCamposProj =  ((char *)query->proj->prim->inf)[0] == '*' ? objeto.qtdCampos : query->proj->tam;
    }

    tupla *pagina = getPage(bufferpoll, esquema, objeto, 0);
    if(!pagina){
        printf("Tabela vazia.\n");
        return NULL;
    }
    if(!validaColsWhere(query->tok, esquema, objeto.qtdCampos)){
        return NULL;
    }
    int k;
    char abortar = 0;
    Lista *resultado = novaLista(NULL);
    for(int p = 0; !abortar && bufferpoll[p].nrec; p++) {
        pagina = getPage(bufferpoll, esquema, objeto, p);
        if(pagina == ERRO_PARAMETRO){
            printf("ERROR: could not open the table.\n");
            return NULL;
        }
        for(k = 0; !abortar && k < bufferpoll[p].nrec; k++){
            tupla *currentTuple = &pagina[k];
            char satisfies = 0;

            if(query->tok){
                Lista *l = resArit(query->tok, currentTuple);
                if(l) {
                    Lista *l2 = relacoes(l);
                    satisfies = logPosfixa(l2);
                }
                else abortar = 1;
            }
            else satisfies = 1;
            if(!abortar && satisfies) {
                tupla *t = (tupla*)uffslloc(sizeof(tupla));
                memcpy(t, currentTuple, sizeof(tupla));
                (tipo == 's') ? adcResultado(resultado, currentTuple, indiceProj, qtdCamposProj) : adcNodo(resultado, resultado->ult, t);
            }
        }

    }
    if(abortar) resultado = NULL;
    

    return resultado;
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Copia todas as informações menos a tabela do objeto, que será removida.
    Parametros: Objeto que será removido do schema.
    Retorno:    INT
                SUCCESS,
                ERRO_REMOVER_ARQUIVO_SCHEMA
   ---------------------------------------------------------------------------------------------*/

int procuraSchemaArquivo(struct fs_objects objeto){
    FILE *schema, *newSchema;
    int cod = 0;
    char *tupla = (char *)uffslloc(sizeof(char) * 109);
    memset(tupla, '\0', 109);

    tp_table *esquema = (tp_table *)uffslloc(sizeof(tp_table)*objeto.qtdCampos);
    memset(esquema, 0, sizeof(tp_table)*objeto.qtdCampos);

    char directory[LEN_DB_NAME_IO];
    memset(&directory, '\0', LEN_DB_NAME_IO);

    strcpy(directory, connected.db_directory);
    strcat(directory, "fs_schema.dat");

    if((schema = fopen(directory, "a+b")) == NULL) {
        return ERRO_REMOVER_ARQUIVO_SCHEMA;
    }

    strcpy(directory, connected.db_directory);
    strcat(directory, "fs_nschema.dat");

    if((newSchema = fopen(directory, "a+b")) == NULL) {
        return ERRO_REMOVER_ARQUIVO_SCHEMA;
    }

    fseek(newSchema, 0, SEEK_SET);

    while((fgetc (schema) != EOF)){ // Varre o arquivo ate encontrar todos os campos com o codigo da tabela.
        fseek(schema, -1, 1);
        fseek(newSchema, 0, SEEK_END);

        if(fread(&cod, sizeof(int), 1, schema)){ // Le o codigo da tabela.
            if(cod != objeto.cod){
                fwrite(&cod, sizeof(int), 1, newSchema);

                fread(tupla, sizeof(char), TAMANHO_NOME_CAMPO, schema);
                strcpylower(esquema[0].nome,tupla);                  // Copia dados do campo para o esquema.
                fwrite(tupla, sizeof(char), TAMANHO_NOME_CAMPO, newSchema);

                fread(&esquema[0].tipo , sizeof(char), 1 , schema);
                fread(&esquema[0].tam  , sizeof(int) , 1 , schema);
                fread(&esquema[0].chave, sizeof(int) , 1 , schema);

                fwrite(&esquema[0].tipo , sizeof(char), 1, newSchema);
                fwrite(&esquema[0].tam  , sizeof(int) , 1, newSchema);
                fwrite(&esquema[0].chave, sizeof(int) , 1, newSchema);

                fread(tupla, sizeof(char), TAMANHO_NOME_TABELA, schema);
                strcpylower(esquema[0].tabelaApt,tupla);
                fwrite(&esquema[0].tabelaApt, sizeof(char), TAMANHO_NOME_TABELA, newSchema);

                fread(tupla, sizeof(char), TAMANHO_NOME_CAMPO, schema);
                strcpylower(esquema[0].attApt,tupla);
                fwrite(&esquema[0].attApt, sizeof(char), TAMANHO_NOME_CAMPO, newSchema);

            } else {
                fseek(schema, 109, 1);
            }
        }
    }

    fclose(newSchema);
    fclose(schema);

    char directoryex[LEN_DB_NAME_IO*2];
    memset(&directoryex, '\0', LEN_DB_NAME_IO*2);
    strcpy(directoryex, connected.db_directory);
    strcat(directoryex, "fs_schema.dat");

    remove(directoryex);

    strcpy(directoryex, "mv ");
    strcat(directoryex, connected.db_directory);
    strcat(directoryex, "fs_nschema.dat ");
    strcat(directoryex, connected.db_directory);
    strcat(directoryex, "fs_schema.dat");

    system(directoryex);

    return SUCCESS;
}

/* ----------------------------------------------------------------------------------------------
    Objetivo:   Função para exclusão de tabelas.
    Parametros: Nome da tabela (char).
    Retorno:    INT
                SUCCESS,
                ERRO_REMOVER_ARQUIVO_OBJECT,
                ERRO_REMOVER_ARQUIVO_SCHEMA,
                ERRO_LEITURA_DADOS.
   ---------------------------------------------------------------------------------------------*/

int excluirTabela(char *nomeTabela) {
    struct fs_objects objeto, objeto1;
    tp_table *esquema, *esquema1;
    int x,erro, i, j, k, l, qtTable;
	  char str[20];
    char dat[5] = ".dat";
    FILE *f = NULL;
    memset(str, '\0', 20);

    if (!verificaNomeTabela(nomeTabela)) {
        printf("ERROR: table \"%s\" does not exist.\n", nomeTabela);
        return ERRO_NOME_TABELA;
    }

    strcpylower(str, nomeTabela);
    strcat(str, dat);              //Concatena e junta o nome com .dat

    abreTabela(nomeTabela, &objeto, &esquema);
    qtTable = quantidadeTabelas();

    char **tupla = (char **)uffslloc(sizeof(char **)*qtTable);

    memset(tupla, 0, qtTable);

    for (i=0; i < qtTable; i++) {
        tupla[i] = (char *)uffslloc(sizeof(char)*TAMANHO_NOME_TABELA);
        memset(tupla[i], '\0', TAMANHO_NOME_TABELA);
    }
    tp_table *tab2 = (tp_table *)uffslloc(sizeof(struct tp_table));
    tab2 = procuraAtributoFK(objeto);   //retorna o tipo de chave que e cada campo
    FILE *dicionario;
    char directory[LEN_DB_NAME_IO*2];
    memset(directory, '\0', LEN_DB_NAME_IO*2);
    strcpy(directory, connected.db_directory);
    strcat(directory, "fs_object.dat");

    if((dicionario = fopen(directory,"a+b")) == NULL)
        return ERRO_ABRIR_ARQUIVO;
    k=0;
    while(fgetc (dicionario) != EOF){
        fseek(dicionario, -1, 1);
        //coloca o nome de todas as tabelas em tupla
        fread(tupla[k], sizeof(char), TAMANHO_NOME_TABELA , dicionario);
        k++;
        fseek(dicionario, 32, 1);
    }
    fclose(dicionario);
    for(i = 0; i < objeto.qtdCampos; i++){
        if(tab2[i].chave == PK){
            for(j=0; j<qtTable; j++) {                      //se tiver chave primaria verifica se ela e chave
                if(objcmp(tupla[j], nomeTabela) != 0) {     //estrangeira em outra tabela

                    abreTabela(tupla[j], &objeto1, &esquema1);

                    tp_table *tab3 = (tp_table *)uffslloc(sizeof(struct tp_table));
                    tab3 = procuraAtributoFK(objeto1);

                    for(l=0; l<objeto1.qtdCampos; l++) {
                        if(tab3[l].chave == FK) { //verifica se a outra tabela possui chave estrangeira. se sim, verifica se e da tabela anterior.
                            if(objcmp(nomeTabela, tab3[l].tabelaApt) == 0) {
                                printf("ERROR: cannot drop table \"%s\" because other objects depend on it.\n", nomeTabela);
                                return ERRO_CHAVE_ESTRANGEIRA;
                            }
                        }
                    }
                }
            }
        }
    }

    for (i = 0; objeto.qtdIndice != 0; i++) {
      if(tab2[i].chave == PK || tab2[i].chave == BT) {
        strcpy(directory, connected.db_directory);
        strcat(directory, nomeTabela);
        strcat(directory, tab2[i].nome);
        strcat(directory, dat);
        if ((f = fopen(directory,"r")) != NULL){
    			remove(directory);
          fclose(f);
    		}
        objeto.qtdIndice--;
      }
    }

    tp_buffer *bufferpoll = initbuffer();
    if(bufferpoll == ERRO_DE_ALOCACAO){
        printf("ERROR: no memory available to allocate buffer.\n");
        return ERRO_LEITURA_DADOS;
    }

    erro = SUCCESS;
    for(x = 0; erro == SUCCESS; x++)
        erro = colocaTuplaBuffer(bufferpoll, x, esquema, objeto);

    if(procuraSchemaArquivo(objeto) != 0) {
        return ERRO_REMOVER_ARQUIVO_SCHEMA;
    }

    if(procuraObjectArquivo(nomeTabela) != 0) {
        return ERRO_REMOVER_ARQUIVO_OBJECT;
    }
   	strcpy(directory, connected.db_directory);
    strcat(directory, str);
    remove(directory);

    printf("DROP TABLE\n");
    return SUCCESS;
}

/////
int verifyFieldName(char **fieldName, int N){
    int i, j;
    if(N<=1) return 1;
    for(i=0; i < N; i++){
        for(j=i+1; j < N; j++){
            if(objcmp(fieldName[i], fieldName[j]) == 0){
                printf("ERROR: column \"%s\" specified more than once\n", fieldName[i]);
                return 0;
            }
        }
    }
    return 1;
}

//////
void createTable(rc_insert *t) {
  if(strlen(t->objName) > TAMANHO_NOME_TABELA){
      printf("A table name must have no more than %d caracteres.\n",TAMANHO_NOME_TABELA);
      return;
  }
  int size;
  strcpylower(t->objName, t->objName);        //muda pra minúsculo
  char *tableName = (char*) uffslloc(sizeof(char)*(TAMANHO_NOME_TABELA+10)),
                    fkTable[TAMANHO_NOME_TABELA], fkColumn[TAMANHO_NOME_CAMPO];
    ushort codFK;

  strncpylower(tableName, t->objName, TAMANHO_NOME_TABELA);
  strcat(tableName, ".dat\0");                  //tableName.dat
  if(existeArquivo(tableName)){
    printf("ERROR: table already exist\n");
    return;
  }

  table *tab = NULL;
  tab = iniciaTabela(t->objName);    //cria a tabela
  if(0 == verifyFieldName(t->columnName, t->N)){
    freeTable(tab);
    return;
  }
  int i;
  int PKcount = 0;
  for(i = 0; i < t->N; i++){
    if(t->type[i] == 'S')
  		size = atoi(t->values[i]);
  	else if(t->type[i] == 'I')
  		size = sizeof(int);
  	else if(t->type[i] == 'D')
  		size = sizeof(double);
    else if(t->type[i] == 'C')
  		size = sizeof(char);
    
    if (size <= 0)
    {
        printf("ERROR: attribute \"%s\" has invalid size (%d)\n", t->columnName[i], size);
        freeTable(tab);
        return;
    }

    if(t->attribute[i] == PK) {
        PKcount++;
        if(PKcount > 1) {
            printf("multiple primary keys for table \"%s\" are not allowed\n",t->objName);
            freeTable(tab);
            return;
        }
    }
    
  	if(t->attribute[i] == FK){
  		strncpylower(fkTable, t->fkTable[i], TAMANHO_NOME_TABELA);
        codFK = 1;
  		strncpylower(fkColumn,t->fkColumn[i], TAMANHO_NOME_CAMPO);
  	}
    else{
  		strcpy(fkTable, "");
  		strcpy(fkColumn, "");
  	}
    tab = adicionaCampo(tab, t->columnName[i], t->type[i], size, t->attribute[i], fkTable, fkColumn, codFK);
    if((objcmp(fkTable, "") != 0) || (objcmp(fkColumn, "") != 0)){
      if(verifyFK(fkTable, fkColumn) == 0){
  		  printf("ERROR: attribute FK cannot be referenced\n");
        freeTable(tab);
        return;
      }
    }
  }

  //Se não existe tabela com esse nome
  if(finalizaTabela(tab) == SUCCESS) {
  	for(int i = 0; i < t->N; i++) {
  		if(t->attribute[i] == PK) { //procura o atributo PK e cria o arquivo de índice
        char *aux_nome_index = NULL;
  		  aux_nome_index = (char *)uffslloc(strlen(connected.db_directory) + strlen(t->objName) + strlen(t->columnName[i]));
        strcpy(aux_nome_index, connected.db_directory);
        strcat(aux_nome_index, t->objName);
  		  strcat(aux_nome_index, t->columnName[i]);
        inicializa_indice(aux_nome_index);
        break;
  		}
  	}
  	printf("CREATE TABLE\n");
  } else { //Tabela já existe, então não é preciso criar o índice b+.
	  printf("ERROR: table already exist\n");
  }

  if(tab != NULL) freeTable(tab);
}

void createIndex(rc_insert *t) {
  struct fs_objects obj;
  struct tp_table   *tb;
  char dir[TAMANHO_NOME_TABELA + TAMANHO_NOME_ARQUIVO + TAMANHO_NOME_CAMPO + TAMANHO_NOME_INDICE];
  int flag = 0;
  FILE *f = NULL;

  if (!verificaNomeTabela(t->objName)) {
    printf("ERROR: table \"%s\" does not exist.\n", t->objName);
    return;
  }

  obj = leObjeto(t->objName);
  tb  = leSchema(obj);

  for(tp_table *aux = tb; aux != NULL && !flag; aux = aux->next) {
    if(strcmp(aux->nome, t->columnName[0]) == 0) { //Procura o atributo na tabela
      if(aux->chave == PK) {// Se o atributo é PK já possui índice criado automaticamente
        printf("ERROR: attribute \"%s\" already has a b+ index.\n", t->columnName[0]);
        return;
      }
      flag = 1;
    }
  }

  if (!flag) {
    printf("ERROR: attribute \"%s\" does not exist on table %s.\n", t->columnName[0], t->objName);
    return;
  }

  strcpy(dir, connected.db_directory);
  strcat(dir, t->objName);
  strcat(dir, t->columnName[0]);
  strcat(dir, ".dat");

  if ((f = fopen(dir,"r")) != NULL){
    printf("ERROR: B+ index file already exists.\n");
    return;
  }

  strcpy(dir, connected.db_directory);
  strcat(dir, t->objName);
  strcat(dir, t->columnName[0]);

  inicializa_indice(dir);
  incrementaQtdIndice(t->objName);
  adicionaBT(t->objName, t->columnName[0]);
  printf("CREATE INDEX\n");
}

///////
