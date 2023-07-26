// The MIT License (MIT)
// 
// Copyright (c) 2016 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define FIRST_DATA_BLOCK 1001
#define MAX_FILE_SIZE 1048576

uint8_t data[NUM_BLOCKS][BLOCK_SIZE];

//512 blocks just for free block map
uint8_t * free_blocks;
uint8_t * free_inodes;

//directory
struct directoryEntry
{
  char filename[64];
  short in_use;
  int32_t inode;
}; 

struct directoryEntry * directory;

//inode
struct inode
{
  int32_t blocks[BLOCKS_PER_FILE];
  short in_use;
  uint8_t attribute;
  uint32_t file_size;
  int hr, min, sec; 
};



struct inode * inodes;

FILE *fp;
char image_name[64];
uint8_t image_open;



#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size
#define MAX_NUM_ARGUMENTS 12    // Mav shell only supports 10 arguments
#define MAX_HISTORY_SIZE 15     // Mav shell supports history of size 15

int32_t findFreeBlock()
{
  int i;
  for(i = 0; i < NUM_BLOCKS; i++)
  {
    if(free_blocks[i])
    {
      free_blocks[i] = 0;
      return i + 1001;
    }
  }
  return -1;
}

int32_t findFreeInode()
{
  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    if(free_inodes[i])
    {
      free_inodes[i] = 0;
      return i;
    }
  }
  return -1;
}

int32_t findFreeInodeBlock( int32_t inode)
{
    int i;
    for(i = 0; i < BLOCKS_PER_FILE; i++)
    {
        if(inodes[inode].blocks[i] == -1)
        {
            inodes[inode].blocks[i] = 0;
            return i;
        }
    }
    return -1;
}

void init()
{
    directory   = (struct directoryEntry*)&data[0][0];
    inodes      = (struct inode *)&data[20][0];
    free_blocks = (uint8_t *)&data[1000][0];
    free_inodes = (uint8_t *)&data[19][0]; 

    memset(image_name, 0, 64);
    image_open = 0;
    

    int i;   
    for(i = 0; i < NUM_FILES; i++)
    {
        
        directory[i].in_use = 0;
        directory[i].inode = -1;
        free_inodes[i] = 1;

        memset(directory[i].filename, 0, 64);

        int j;
        for(j = 0; j < NUM_BLOCKS; j++)
        {
            inodes[i].blocks[j] = -1;
            inodes[i].in_use = 0; 
            inodes[i].attribute = 0;
            inodes[i].file_size = 0;

        }
        
    }
    int j;
    for(int j = 0; j < NUM_BLOCKS; j++)
    {
        free_blocks[j] = 1;
    }

}

uint32_t df()
{
    int j;
    int count = 0;
    for(j = FIRST_DATA_BLOCK; j < NUM_BLOCKS; j++)
    {
        if(free_blocks[j])
        {
            count++;
        }
    }
    return (count * BLOCK_SIZE);
}



void createfs(char * filename)
{
  fp = fopen(filename, "w");

  strncpy(image_name, filename, strlen(filename));

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);
  
  image_open = 1;
  
  int i;
  for(i = 0; i< NUM_FILES; i++)
  {
    
    directory[i].in_use = 0;
    directory[i].inode = 1;
    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);

    int j;
    for(j = 0; j < NUM_BLOCKS; j++)
    {
        inodes[i].blocks[j] = -1;
        inodes[i].in_use = 0;
        inodes[i].attribute = 0;
        inodes[i].file_size = 0;
    }
  }

  int j;
  for(j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }
}

void savefs()
{
  if(image_open == 0)
  {
    printf("ERROR: Disk img not open\n");
  }

  fp = fopen(image_name, "w");

  fwrite( &data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
  memset(image_name, 0, 64);
}

void openfs(char * filename)
{    
  fp = fopen(filename, "r");
  
  strncpy(image_name, filename, strlen(filename));

  fread(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);

  image_open = 1;
}

void closefs()
{
  if (image_open == 0)
  {
    printf("ERROR: Disk image is not open\n");
    return;
  }

  fclose(fp);
  
  image_open = 0; 
  memset(image_name, 0, 64);
  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);
}

void list(char * attrib)
{
    int i;
    int not_found = 1;
    
    for(i = 0; i < NUM_FILES; i++)
    {

        if(directory[i].in_use)
        {
            not_found=0;
            char filename[65];
            memset(filename, 0, 65);
            strncpy(filename, directory[i].filename, strlen(directory[i].filename));

            if(inodes[directory[i].inode].hr < 0)
            {
                inodes[directory[i].inode].hr += 24;
            }
            if(inodes[directory[i].inode].hr > 12)
            {
                inodes[directory[i].inode].hr = inodes[i].hr % 12;
            }

            if(!strcmp(attrib, "-h"))
            {
                printf("%s   %d   %02d:%02d:%02d\n",filename,inodes[directory[i].inode].file_size,
                inodes[directory[i].inode].hr, inodes[directory[i].inode].min,
                inodes[directory[i].inode].sec);
            }
            else if(!strcmp(attrib, "-a"))
            {
                printf("%s   %d   %02d:%02d:%02d   ",filename,inodes[directory[i].inode].file_size,
                inodes[directory[i].inode].hr, inodes[directory[i].inode].min, 
                inodes[directory[i].inode].sec);
                for (int j = 7; j >= 0; j--) 
                {
                    printf("%d", (inodes[directory[i].inode].attribute >> j) & 1);
                }
                printf("\n");
            }
            else
            {
                if(inodes[directory[i].inode].attribute != 1)
                {
                    printf("%s   %d   %02d:%02d:%02d\n",filename,inodes[directory[i].inode].file_size,
                    inodes[directory[i].inode].hr, inodes[directory[i].inode].min,
                    inodes[directory[i].inode].sec);
                }
            }
        }
    }
    if(not_found)
    {
        printf("ERROR: No file found\n");
    }
}

void insert (char* filename)
{
    // verify the filename isnt null
    if (filename == NULL)
    {
        printf("ERROR: filename is NULL\n");
        return;
    }

    // verify the file exists
    struct stat buf;
    int ret = stat(filename, &buf);

    if(ret == -1)
    {
        printf("ERROR: File does not exist.\n");
        return;
    }

    // verify the file isn't too big
    if(buf.st_size > MAX_FILE_SIZE)
    {
        printf("ERROR: File is too large.\n");
        return;
    }

    // verify there is enough space
    if(buf.st_size > df())
    {
        printf("ERROR: Not enough free disk space.\n");
        return;
    }

    // find an empty directory entry
    int i;
    int directory_entry = -1;
    for(i = 0; i < NUM_FILES; i++)
    {
        if(directory[i].in_use == 0)
        {
          directory_entry = i;

          break;
        }
    }
    
    if(directory_entry == -1)
    {
        printf("ERROR: Could not find a free directory entry\n");
        return;
    }

    // Open the input file read-only 
    FILE *ifp = fopen ( filename, "r" ); 
    printf("Reading %d bytes from %s\n", (int) buf.st_size, filename );
    
    // Save off the size of the input file since we'll use it in a couple of places and 
    // also initialize our index variables to zero. 
    int32_t copy_size   = buf.st_size;
   // Bytes_File = copy_size + Bytes_File;

    time_t temp_time = time(0);
    struct tm *temp_info = localtime(&temp_time);

    // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
    // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
    // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
    int32_t offset   = 0;               

    // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
    // memory pool. Why? We are simulating the way the file system stores file data in
    // blocks of space on the disk. block_index will keep us pointing to the area of
    // the area that we will read from or write to.
    int32_t block_index = -1;

    // find a free inode
    int32_t inode_index = findFreeInode();
    if(inode_index == -1)
    {
      printf("ERROR: Can not find a free inode.\n");
      return;
    }

    free_inodes[inode_index] = 0;

    // place the file infor in the directory
    inodes[inode_index].file_size = buf.st_size;
    inodes[inode_index].hr = temp_info->tm_hour - 5;
    inodes[inode_index].min =  temp_info->tm_min;
    inodes[inode_index].sec = temp_info->tm_sec;

    directory[directory_entry].in_use = 1;
    directory[directory_entry].inode = inode_index;
    strncpy(directory[directory_entry].filename, filename, strlen(filename));

    // copy_size is initialized to the size of the input file so each loop iteration we
    // will copy BLOCK_SIZE byt/es from the file then reduce our copy_size counter by
    // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
    // we have copied all the data from the input file.
    while( copy_size > 0 )
    {
        fseek( ifp, offset, SEEK_SET );
    
        // Read BLOCK_SIZE number of bytes from the input file and store them in our
        // data array. 

        // find a free block
        block_index = findFreeBlock();

        if(block_index == -1)
        {
            printf("ERROR: Can not find a free block.\n");
            return;
        }   

        int32_t bytes  = fread( data[block_index], BLOCK_SIZE, 1, ifp );

        // save the block in the inode
        int32_t inode_block = findFreeInodeBlock(inode_index);
        inodes[inode_index].blocks[inode_block] = block_index;
        free_blocks[block_index] = 0;

        // If bytes == 0 and we haven't reached the end of the file then something is 
        // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
        // It means we've reached the end of our input file.
        if( bytes == 0 && !feof( ifp ) )
        {
            printf("ERROR: An error occured reading from the input file.\n");
            return;
        }

        // Clear the EOF file flag.
        clearerr( ifp );

        // Reduce copy_size by the BLOCK_SIZE bytes.
        copy_size -= BLOCK_SIZE;
        
        // Increase the offset into our input file by BLOCK_SIZE.  This will allow
        // the fseek at the top of the loop to position us to the correct spot.
        offset    += BLOCK_SIZE;

        block_index = findFreeBlock();
    }

    // We are done copying from the input file so close it out.
    fclose( ifp );

}

void Delete(char * filename)   
// We have the name of the file, this function only works if we have a filesystem open
{
    int i =0; 

    while(strcmp(directory[i].filename, filename) && i < NUM_FILES)
    {
        i++; 
    }

    int32_t location = (directory[i].inode);
    directory[i]. in_use = 0;

    inodes[location].in_use = 0;

    i =0;
    int32_t index =  inodes[location].blocks[i];

    while((index != 0) && (i < BLOCKS_PER_FILE) )
    {  
        free_blocks[index] = 1;
        i++;
        index = inodes[location].blocks[i];
    }
}


void Undelete (char * filename)
{
  int i =0; 
  while(strcmp((directory[i].filename), filename) && i < NUM_FILES)
  { 
    i++;
  }

  if(strcmp((directory[i].filename), filename))
  {
    printf("undelete: Can not find the file.\n"); 
  }
  else
  {
    directory[i].in_use = 1;

    int32_t location = (directory[i].inode);

    inodes[location].in_use = 1;

    i = 0;
    int32_t index =  inodes[location].blocks[i];

    while((index != 0) && (i < BLOCKS_PER_FILE) )
    {  
      free_blocks[index] = 0;
      i++;
      index = inodes[location].blocks[i];
    }
  }
}
  
void attrib(char * attribute, char * filename)
{
    uint8_t x;

    for(int i = 0; i < NUM_FILES; i++)
    {
        if(!strcmp(filename, directory[i].filename))
        {
            if(attribute[0] == '+')
            {
                if(attribute[1] == 'r')
                {
                    x = 2;
                }
                else if(attribute[1] == 'h')
                {
                    x = 1;
                }
                inodes[i].attribute = x;
            }
            else if(!strcmp(attribute, "-h") || !strcmp(attribute, "-r"))
            {
                inodes[i].attribute = 0;
            }
        }
    }
}

void readFile(char * filename, int start_byte, int num_bytes)
{
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(!strcmp(filename, directory[i].filename))
        {
            int end_byte = start_byte + num_bytes;
            int start_block = start_byte / BLOCK_SIZE;
            int end_block = end_byte / BLOCK_SIZE;

            for(int j = start_block; j <= end_block; j++)
            {
                if(start_block == end_block)
                {
                    for(int k = start_byte % BLOCK_SIZE; k < end_byte % BLOCK_SIZE; k++)
                    {
                        printf("%x\n", data[inodes[directory[i].inode].blocks[j]][k]);
                    }
                }
                else if(j == end_block)
                {
                    for(int k = 0; k < end_byte % BLOCK_SIZE; k++)
                    {
                        printf("%x\n", data[inodes[directory[i].inode].blocks[j]][k]);
                    }

                }
                else if(j == start_block)
                {
                    for(int k = start_byte % BLOCK_SIZE; k < BLOCK_SIZE; k++)
                    {
                        printf("%x\n", data[inodes[directory[i].inode].blocks[j]][k]);
                    }
                }
                else
                {
                    for(int k = 0; k < BLOCK_SIZE; k++)
                    {
                        printf("%x\n", data[inodes[directory[i].inode].blocks[j]][k]);
                    }
                }
                
            }
        }
    }
}

void encryptFile(char * filename, char cipher)
{
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(!strcmp(filename, directory[i].filename))
        {
            int start_byte = 0;
            int end_byte = inodes[directory[i].inode].file_size;
            int start_block = 0;
            int end_block = end_byte / BLOCK_SIZE;

            for(int j = start_block; j <= end_block; j++)
            {
                if(start_block == end_block)
                {
                    for(int k = start_byte % BLOCK_SIZE; k < end_byte % BLOCK_SIZE; k++)
                    {
                        data[inodes[directory[i].inode].blocks[j]][k] = 
                        (uint8_t)(((char) data[inodes[directory[i].inode].blocks[j]][k]) ^ cipher);
                    }
                }
                else if(j == end_block)
                {
                    for(int k = 0; k < end_byte % BLOCK_SIZE; k++)
                    {
                        data[inodes[directory[i].inode].blocks[j]][k] = 
                        (uint8_t)(((char) data[inodes[directory[i].inode].blocks[j]][k]) ^ cipher);
                    }

                }
                else if(j == start_block)
                {
                    for(int k = start_byte % BLOCK_SIZE; k < BLOCK_SIZE; k++)
                    {
                        data[inodes[directory[i].inode].blocks[j]][k] = 
                        (uint8_t)(((char) data[inodes[directory[i].inode].blocks[j]][k]) ^ cipher);
                    }
                }
                else
                {
                    for(int k = 0; k < BLOCK_SIZE; k++)
                    {
                        data[inodes[directory[i].inode].blocks[j]][k] = 
                        (uint8_t)(((char) data[inodes[directory[i].inode].blocks[j]][k]) ^ cipher);
                    }
                }
                
            }
        }
    }
}

void retrieve(char* filename)
{
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(!strcmp(filename, directory[i].filename))
        {
            FILE* ofp;
            ofp = fopen(filename, "w");

            if(ofp == NULL)
            {
                printf("Could not open output file: %s\n", filename);
                perror("Opening output file returned");
                return;
            }
            //inodes[directory[i].inode].blocks[0] = block_index;
            int block_index = 0;
            int retrieve_size = inodes[directory[i].inode].file_size;
            int offset = 0;

            printf("Retrieving %d bytes to %s\n", (int) inodes[directory[i].inode].file_size, filename);

            while(retrieve_size > 0)
            {
                int num_bytes;

                if(retrieve_size < BLOCK_SIZE)
                {
                    num_bytes = retrieve_size;
                }
                else 
                {
                    num_bytes = BLOCK_SIZE;
                }

                fwrite(data[inodes[directory[i].inode].blocks[block_index]], num_bytes, 1, ofp);
                
                retrieve_size -= BLOCK_SIZE;
                offset += BLOCK_SIZE;
                block_index++;

                fseek(ofp, offset, SEEK_SET);
            }
            
            fclose(ofp);
            printf("File retrieved!\n");
        }
    }
}

void retrieveExt(char* filename, char* outputfname)
{
    for(int i = 0; i < NUM_FILES; i++)
    {
        if(!strcmp(filename, directory[i].filename))
        {
            FILE* ofp;
            ofp = fopen(outputfname, "w");

            if(ofp == NULL)
            {
                printf("Could not open output file: %s\n", outputfname);
                perror("Opening output file returned");
                return;
            }
            //inodes[directory[i].inode].blocks[0] = block_index;
            int block_index = 0;
            int retrieve_size = inodes[directory[i].inode].file_size;
            int offset = 0;

            printf("Retrieving %d bytes to %s\n", (int) inodes[directory[i].inode].file_size, outputfname);

            while(retrieve_size > 0)
            {
                int num_bytes;

                if(retrieve_size < BLOCK_SIZE)
                {
                    num_bytes = retrieve_size;
                }
                else 
                {
                    num_bytes = BLOCK_SIZE;
                }

                fwrite(data[inodes[directory[i].inode].blocks[block_index]], num_bytes, 1, ofp);
                
                retrieve_size -= BLOCK_SIZE;
                offset += BLOCK_SIZE;
                block_index++;

                fseek(ofp, offset, SEEK_SET);
            }
            
            fclose(ofp);
            printf("File retrieved!\n");
        }
    }
}

int main()
{

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

  fp = NULL;

  init();
  
  while(1)
  {
    // Print out the msh prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while(!fgets(command_string, MAX_COMMAND_SIZE, stdin));
  
    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for(int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      token[i] = NULL;
    }

    int token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;                                         
                                                           
    char *working_string  = strdup( command_string );                

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
        token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
        if( strlen( token[token_count] ) == 0 )
        {
            token[token_count] = NULL;
        }
        token_count++;
    }
    

    // process the filesystem commands
    if(strcmp("createfs", token[0]) == 0)
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified\n");
            continue;
        }
        createfs(token[1]);
    }
    else if(!strcmp("savefs", token[0]))
    {
        savefs();
    }
    else if(!strcmp("open", token[0]))
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified\n");
            continue;
        }
        openfs(token[1]);
    }
    else if(!strcmp("close", token[0]))
    {
        closefs();
    }
    else if(!strcmp("list", token[0]))
    {
       
         if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }
        if(token[1])
        {
            list(token[1]);
        }
        else
        {
            list("nothing");
        } 
    }
    else if(!strcmp("df", token[0]))
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }

        printf("%d bytes free\n", df());
    }
    else if(!strcmp(token[0], "quit") || !strcmp(token[0], "exit"))
    {
        exit(0);
    }
    else if(!strcmp("insert", token[0]))
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }

        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified");
            continue;
        }

        insert(token[1]);
    }
    else if(!strcmp("delete",token[0]))
    {
      
      int i;

      for( i = 0; i<NUM_FILES;i++)
      {
        if(!(strcmp(token[1],directory[i].filename)))
        {
          break;
        }
      }

      if(inodes[i].attribute != 2)
      {
        if(!image_open)
        {
          printf("ERROR: Disk image is not opened.\n");
          continue;
        }

        if(token[1] == NULL)
        {
         printf("ERROR: No file specified\n");
         continue;
        }

        Delete(token[1]);
      }
      else
      {
        printf("ERROR: Can't delete an read-only file\n");
      }

    }
    else if(!(strcmp("undel",token[0])))
    {
        if(!image_open)
        {
          printf("ERROR: Disk image is not opened.\n");
          continue;
        }

        if(token[1] == NULL)
        {
         printf("ERROR: No file specified\n");
         continue;
        }

        Undelete(token[1]);
    }
    else if(!strcmp("attrib", token[0]))
    {
        if(token[1] && token[2])
        {
            attrib(token[1], token[2]);
        }
        else
        {
            printf("GIVE ME RIGHT # PARAMETERS!!!!!\n");
        }
    }
    else if(!strcmp("read", token[0]))
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }

        if(token[1] == NULL)
        {
            printf("ERROR: No file given.\n");
            continue;
        }

        if(token[2] == NULL)
        {
            printf("ERROR: No start byte given.\n");
            continue;
        }


        if(token[3] == NULL)
        {
            printf("ERROR: No end byte given.\n");
            continue;
        }

        readFile(token[1], atoi(token[2]), atoi(token[3]));

    }
    else if(!strcmp("encrypt", token[0]) || !strcmp("decrypt", token[0]))
    {
        if(!image_open)
        {
            printf("ERROR: Disk image is not opened.\n");
            continue;
        }

        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified.\n");
            continue;
        }

        if(token[2] == NULL)
        {
            printf("ERROR: No cipher specified.\n");
            continue;
        }

        if(strlen(token[2]) > 1)
        {
            printf("Error: Cipher must be only one byte in length.\n");
            continue;
        }

        encryptFile(token[1], *token[2]);
    }
    else if(!strcmp("retrieve", token[0]))
    {
        if(token[1] == NULL)
        {
            printf("ERROR: No filename specified.\n");
            continue;
        }
        else if(token[1] && (token[2] == NULL))
        {
            retrieve(token[1]);
            continue;
        }
        else
        {
            retrieveExt(token[1], token[2]);
        }
    }
    else
    {
        printf("Error: Invalid command.\n");
    }



    // Cleanup allocated memory
    for(int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
        if(token[i] != NULL)
        {
            free(token[i]);
        }
    }

    if(token[0] != NULL && !strcmp(token[0], "cd") && token[1] != NULL)
    {
      // chdir returns 0 on success, and -1 on failure. Print error if failure
      if(chdir(token[1]))
      {
        printf("cd %s: Command not found.\n", token[1]);
      }
    }
  }
  return 0;
  // e2520ca2-76f3-90d6-0242ac120003
 
}
