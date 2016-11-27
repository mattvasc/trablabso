/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096

unsigned short fat[65536]; /*TABELA FAT*/

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

typedef struct {
  char used;
  char *buffer;
  int indice_bloco;
  int bloco_atual;
  int byte_atual;
  int mode;
  int dir;
} open_file;

dir_entry dir[128]; /*COLEÇÃO DE DIRETÓRIOS*/
open_file arq_aberto[128];


/*Inicia o sistema de arquivos e suas estruturas internas. Esta
função é automaticamente chamada pelo interpretador de comandos no
início do sistema. Esta função deve carregar dados do disco para restaurar
um sistema já em uso e é um bom momento para verificar se o disco
está formatado.*/

int fs_init()
{
  char *buffer;
  buffer = (char *) fat;
  int c;
  for(c = 0; c < 256; c++) /*32 blocos * 4k / 8 blocos per sector */
    bl_read(c, buffer + c * 512);
  for(c = 0; c < 32; c++)
    if(fat[c] != 3)
      return fs_format();

  if(fat[c] != 4)
    return fs_format();
  for(c=0;c<128;c++)
    arq_aberto[c].used = 0;

  // Carregando o diretório para memória para outras aplicações
  buffer = (char *) dir;

  for(c = 0; c < 8; c++)
    bl_read(c+256, buffer + c * 512);

  return 1;
}
/*Inicia o dispositivo de disco para uso, iniciando e escre-
vendo as estruturas de dados necessárias.*/
/*VERIFICAR O RETORNO DO bl_write*/
int fs_format() {
  int c;
  char *buffer = (char * ) fat;
  puts("Formatando o disco...");
  /*Referenciando a FAT*/
  for(c = 0; c < 32; c++)
    fat[c] = 3;

  fat[c++] = 4; /*Referenciando o Diretorio*/
  for(;c<bl_size()/8; c++) /*Referenciando blocos livres ate apenas o tamanho do disco*/
    fat[c] = 1;
  for(c = 0; c < 256; c++) /*32 bloco da fat * 8 setores por bloco */
    bl_write(c, (buffer+ c * 512)); /*para ler de 512byte em 512byte*/

  memset(dir,0,sizeof(dir_entry)*128); /*Zerando a arvore de diretorios*/
  buffer = (char *) dir; /*parando em cima do DIR*/
  for(c = 0; c < 8; c++) /*4K / 512 (SECTORSIZE)*/
    bl_write(c+256, buffer + c*512);

  puts("Disco formatado.");
  return 1;
}
/*Retorna o espaço livre no dispositivo em bytes.*/
int fs_free() {
  unsigned c, count;
  for(c=33, count=0 ;c<bl_size()/8;c++)
    if(fat[c]==1)
      count++;
  return count*4096;
}
/*Lista os arquivos do diretório, colo-
cando a saída formatada em buffer . O formato é simples, um arquivo
por linha, seguido de seu tamanho e separado por dois tabs. Observe
que a sua função não deve escrever na tela.*/
int fs_list(char *buffer, int size) { // ARRUMAR PARA VER OVERFLOW NA STRING
  int i, imprimiu, count=0;
  for(i = 0; i < 128; i++){
    if(dir[i].used){
      imprimiu = sprintf(buffer,"%s\t\t%d\n", dir[i].name, dir[i].size);
      buffer+=imprimiu;
      count+=imprimiu; // Se deu overflow na string
      if(count>=4096)
        return 0; // Deu erro
    }
  }
  buffer='\0';
  return 1;
}

/*Cria um novo arquivo com nome
file_name e tamanho 0. Um erro deve ser gerado se o arquivo já existe.*/
int fs_create(char* file_name) {
  int c,d,e;
  char *buffer;
  for(c=0,d=-1; c<128; c++)
  {
    if(dir[c].used && !strcmp(file_name,dir[c].name)) {
      puts("Arquivo já existente");
      return 0;
    }

    else if(!dir[c].used && d ==-1 )
      d = c;
  }

  for(e = 33; e < bl_size()/8; e++)
    if(fat[e]==1){
      strcpy(dir[d].name, file_name);
      dir[d].used = 1;
      dir[d].size = 0;
      dir[d].first_block = e;
      fat[e] = 2; // Marcando como fim de bloco
      buffer = (char*)fat;
      bl_write(e/256, buffer + (e/256)*512); /* Salvando apenas o bloco da FAT Alterada.*/
      buffer = (char *) dir; /*parando em cima do DIR*/
      bl_write(256 + d/16, buffer + (d/16)*512); /*Salvando apenas o bloco do DIR alterado*/
      return d;
    }
  perror("Disco cheio!\n");
  return 0;
}

/*Remove o arquivo com nome
file_name . Um erro deve ser gerado se o arquivo não existe.*/
int fs_remove(char *file_name){
  int i,prox, temp;
  unsigned short posicaoInicialFAT;
  char existe = 0, *buffer;
  /*busco o nome do arquivo e salvo o primeiro bloco*/
  for(i = 0; i < 128; i++)
    if(dir[i].used && !strcmp(dir[i].name, file_name)){
      dir[i].used = 0;
      posicaoInicialFAT = dir[i].first_block;
      existe = 1;
      break;
    }
  if(!existe){
    puts("Arquivo nao existe!");
    return 0;
  }else{
    /*limpo no fat os ponteiros para o arquivo, marcando como 1 == livre*/
    prox = fat[posicaoInicialFAT];
    fat[posicaoInicialFAT] = 1;
    while(prox != 2){
      temp = fat[prox];
      fat[prox] = 1;
      prox = temp;
    }
    buffer = (char *) dir;
    bl_write(256 + i/16, buffer + (i/16)*512); /*Salvando apenas o bloco do DIR alterado*/

    buffer = (char *) fat;
    for(i = 0; i < 256; i++)  // Escrevendo a FAT inteira no Disco
      bl_write(i, buffer + 512 * i);

    return 1;
  }
}
/*Abre o arquivo file_name
para leitura (se mode for FS_R ) ou escrita (se mode for FS_W ). Ao abrir um
arquivo para leitura, um erro deve ser gerado se o arquivo não existe.
Ao abrir um arquivo para escrita, o arquivo deve ser criado ou um ar-
quivo pré-existente deve ser apagado e criado novamente com tamanho
0. Retorna o identificador do arquivo aberto, um inteiro, ou -1 em caso
de erro.*/
int fs_open(char *file_name, int mode) {
  int c,d,e,achou=0;
  for(c=0,d=-1; c<128; c++)
  {
    if(dir[c].used && !strcmp(file_name,dir[c].name)) {
      if(mode==FS_W)
      {
        fs_remove(file_name);
        e = fs_create(file_name);
      }
      achou = 1;
      e = c;
      break;
    }
  }
  if(!achou){
    if( mode==FS_R)
    {
      printf("Arquivo não encontrado!\n");
      return -1;
    }
    else if(mode==FS_W){ // NAO ACHOU O ARQUIVO NO MODO ESCRITA, CRIANDO ELE DO ZERO
      // CRIA O ARQUIVO NO dir[d];
      e = fs_create(file_name);
    }
  }
  // Abrindo o arquivo
  // no primeiro file[livre] pega os dados de dir[c];
  for(d=0;d<128;d++)
    if(!arq_aberto[d].used){
      arq_aberto[d].used = 1;
      arq_aberto[d].dir = e;
      arq_aberto[d].bloco_atual = dir[e].first_block;
      arq_aberto[d].byte_atual = 0;
      arq_aberto[d].mode = mode;
      arq_aberto[d].indice_bloco = 0;
      arq_aberto[d].buffer = malloc(4096);
      for(e=0;e<8;e++)
        bl_read( dir[c].first_block*8 + e , arq_aberto[d].buffer + e * 512);
      return d;
    }
  return -1;
}
/*Fecha o arquivo dado pelo identificador de arquivo
file . Um erro deve ser gerado se não existe arquivo aberto com este
identificador.*/
int fs_close(int file)  {
  int c;
  if(!arq_aberto[file].used){
    printf("Arquivo não existente\n");
    return 0;
  }
  arq_aberto[file].used = 0;

  if(arq_aberto[file].mode == FS_W)
      for(c=0;c<8;c++)
        bl_write(arq_aberto[file].bloco_atual * 8 + c,arq_aberto[file].buffer + c*512);
  free(arq_aberto[file].buffer);
  return 1;
}
/*Escreve size bytes do
buffer no arquivo aberto com identificador file . Retorna quantidade
de bytes escritos (0 se não escreveu nada, -1 em caso de erro). Um erro
deve ser gerado se não existe arquivo aberto com este identificador ou
caso o arquivo tenha sido aberto para leitura.*/
int fs_write(char *buffer, int size, int file) {
  int c, e, f;
  char *disco_p;
  if(arq_aberto[file].mode != FS_W)
    return -1;

  for (c = 0; c < size; c++)
  {
    if(arq_aberto[file].byte_atual == 4096){
      for(e = 33; e < bl_size()/8; e++)
        if(fat[e]==1){
          fat[e] = 2; // Marcando como fim de bloco
          fat[arq_aberto[file].bloco_atual] = e;
          arq_aberto[file].indice_bloco++;
          disco_p = (char*)fat;
          bl_write(e/256, disco_p + (e/256)*512); /* Salvando apenas o bloco da FAT Alterada.*/
          bl_write(arq_aberto[file].bloco_atual/256, disco_p + (arq_aberto[file].bloco_atual/256)*512);

          for(f = 0; f < 8; f++) {
            bl_write(arq_aberto[file].bloco_atual * 8 + f, arq_aberto[file].buffer + 512 * f);
          }

          arq_aberto[file].byte_atual = 0;
          arq_aberto[file].bloco_atual = e;
          arq_aberto[file].indice_bloco++;

          break;
        }
    }

    arq_aberto[file].buffer[arq_aberto[file].byte_atual++] = buffer[c];
  }

  dir[arq_aberto[file].dir].size += c;
  disco_p = (char*) dir;
  bl_write(256 + arq_aberto[file].dir/16, disco_p + (arq_aberto[file].dir/16)*512); /*Salvando apenas o bloco do DIR alterado*/

  return c;
}
/*Lê no máximo size by-
tes no buffer do arquivo aberto com identificador file . Retorna quan-
tidade de bytes efetivamente lidos (0 se não leu nada, o que indica que o
arquivo terminou, -1 em caso de erro). Um erro deve ser gerado se não
existe arquivo aberto com este identificador ou caso o arquivo tenha sido
aberto para escrita.*/
int vez = 0;
int fs_read(char *buffer, int size, int file) {

  int c, d;
  if(arq_aberto[file].mode != FS_R)
  {
    printf("ERRO: Tentando ler um arquivo aberto para escrita!\n");
    return -1;
  }
  for(c=0;c<size; c++)
  {
      if(arq_aberto[file].indice_bloco * 4096 + arq_aberto[file].byte_atual < dir[arq_aberto[file].dir].size) // Se der para ler !
      {
        if(arq_aberto[file].byte_atual == 4096) // Se chegou no fim do buffer
        {
          if(fat[arq_aberto[file].bloco_atual] != 2) // Olhando para o proximo bloco na fat
          {
            for(d = 0; d < 8; d++) // Carregando para memoria
            {
              bl_read(fat[arq_aberto[file].bloco_atual] * 8 + d, arq_aberto[file].buffer + 512 * d);
            }

            arq_aberto[file].indice_bloco++;
            arq_aberto[file].byte_atual = 0;
            arq_aberto[file].bloco_atual = fat[arq_aberto[file].bloco_atual];
          }
          else // Caso o arquivo arquivo não possua mais blocos:
          {
            return c;
          }
        }
        buffer[c] = arq_aberto[file].buffer[arq_aberto[file].byte_atual++];
      } // EOF:
      else
      {
        arq_aberto[file].indice_bloco++;
        return c;
          //break;
      }

  }
  return c;
}
