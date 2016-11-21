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
  char buffer[4096];
  int bloco_atual;
  int dir;
} open_file;

dir_entry dir[128]; /*COLEÇÃO DE DIRETÓRIOS*/
open_file arq_abertos[128];


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

  for(c=0;c<128;c++)
    file[c].used = 0;

  if(fat[c] != 4)
    return fs_format();
/*
buffer = (char *) dir;

for(c = 0; c < 8; c++)
  bl_read(c+256, buffer + c * 512);*/

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
      return 1;
    }
  perror("Disco cheio!\n");
  return 0;
}

/*Remove o arquivo com nome
file_name . Um erro deve ser gerado se o arquivo não existe.*/
int fs_remove(char *file_name){
  int i,prox;
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
      prox = fat[prox];
      fat[prox] = 1;
    }

    buffer = (char *) fat;

    for(i = 0; i < 256; i++)
      bl_write(i, buffer + 512 * i);

    buffer = (char *) dir;

    for (i = 0; i < 8; i++)
      bl_write(i + 256, buffer + 512 * i);

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
  int c,d,e,achou=0, posicaoInicialFAT, prox;
  for(c=0,d=-1; c<128; c++)
  {
    if(dir[c].used && !strcmp(file_name,dir[c].name)) {
      if(mode==FS_W)
      {
        // Removendo o arquivo:
        posicaoInicialFAT = dir[c].first_block;
        prox = fat[posicaoInicialFAT];
        fat[posicaoInicialFAT] = 1;

        while(prox != 2){
          prox = fat[prox];
          fat[prox] = 1;
        }
        //Definindo tamanho = 0
        dir[c].size = 0;
        fat[ dir[c].first_block ] = 2;

        // "Recriando" o arquivo
      }
      achou = 1;
      break;
    }
    else if(!dir[c].used && d ==-1 )
      d = c; // Primeiro diretório vazio encontrado
  }
  if(!achou){
    if( mode==FS_R)
    {
      printf("Arquivo não encontrado!\n");
      return -1;
    }
    else if(mode==FS_W){ // NAO ACHOU O ARQUIVO NO MODO ESCRITA, CRIANDO ELE DO ZERO
      // CRIA O ARQUIVO NO dir[d];
        if(d==-1)
          return -1; // DIR CHEIO
        for(e = 33, achou=0; e < bl_size()/8; e++)
          if(fat[e]==1){
            dir[d].first_block = e;
            achou = 1;
          }
        if(!achou)
          return -1; // FAT CHEIA
        dir[d].used = 1;
        dir[d].size = 0;
        strcpy(dir[d].name, file_name);
        c = d;
    }
  }
  // Abrindo o arquivo
  // no primeiro file[livre] pega os dados de dir[c];
  for(d=0;d<128;d++)
    if(!file[d].used){
      arq_abertos[d].used = 1;
      arq_abertos[d].dir = c;
      arq_abertos[d].bloco_atual = 0;
      for(e=0;e<8;e++)
        bl_read( dir[c].first_block*8 + e , arq_abertos[d].buffer + e * 512);
      return d;
    }

  // printf("Função não implementada: fs_open\n");
  return -1;
}
/*Fecha o arquivo dado pelo identificador de arquivo
file . Um erro deve ser gerado se não existe arquivo aberto com este
identificador.*/
int fs_close(int file)  {
  if(!arq_abertos[file].used){
    printf("Arquivo não existente\n");
    return 0;
  }
  arq_abertos[file].used = 0;
  return 1;
}
/*Escreve size bytes do
buffer no arquivo aberto com identificador file . Retorna quantidade
de bytes escritos (0 se não escreveu nada, -1 em caso de erro). Um erro
deve ser gerado se não existe arquivo aberto com este identificador ou
caso o arquivo tenha sido aberto para leitura.*/
int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}
/*Lê no máximo size by-
tes no buffer do arquivo aberto com identificador file . Retorna quan-
tidade de bytes efetivamente lidos (0 se não leu nada, o que indica que o
arquivo terminou, -1 em caso de erro). Um erro deve ser gerado se não
existe arquivo aberto com este identificador ou caso o arquivo tenha sido
aberto para escrita.*/
int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}
