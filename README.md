# rsd 目前只在Ubuntu18上测试过，其他Linux版本应该也行
在两个远程文件夹之间，进行文件共享，比如往一个文件夹里拖动文件，这个文件也会复制到远程的文件夹，目前只支持较小的文件，文件较大会产生数据缺失情况，有时间会完善这个bug

1.提供server端和client端的程序，只需要在主目录下执行make即可编译

2.首先，启动server程序，指定服务的ip和端口，比如

./server 127.0.0.1 8080

3.其次，在能访问服务器ip的机器上，执行

./client ip port username password dirname1
./client ip port username password dirname2

4.上述两个命令可以在不同的机器上执行，只要能访问server的程序即可！dirname1和dirname2指定需要同步的目录。

5.扔掉你的移动U盘！


