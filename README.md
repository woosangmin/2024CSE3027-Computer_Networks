1. You should launch this code in a Linux machine.
2. To launch this code, extract all files to one folder.
3. Open a terminal and move to the folder.
4. type myserver "port number". It will not accept port numbers
   below 1024, 8080, 6789.
5. Before launching the webserver, it is recommended to allow the port number you 
  decide to use with ‘firewall-cmd —add-port=0000/tcp —permanent —zone=public’.
  You have to change ‘0000/tcp’ to the port number exactly before using the command.
  If you do not know the zone which the machine uses, then you can get the zone
  with ‘firewall-cmd —list-all’.
6. Please change the UID/GID of files and folders to let the process properly access
  the files and folders.
