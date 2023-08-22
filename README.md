# Rain

A file archiver that uses the drop format of files  

Both reads and writes drop files  
 
## Commands
- Print a simple list of file and directory names archived inside a drop file  
-l [file.drop]  

- Print a detailed list of files and directories including file permissions, droplet format, file size, and file name  
-L [file.drop]  

- Check the hashes of drop files to confirm the correctness  
-C [file.drop]  

- Extract the files  and directories inside a drop into the current directory
-x [file.drop]

- Create a drop from a list of files and directories
-c [file1] [file2] ... [directory1] [directory2] ...
