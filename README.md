# HeapSnap

[点此下载](https://github.com/albuer/heapsnap/releases)

## 1、HeapSnap 是什么

[HeapSnap](https://github.com/albuer/heapsnap) 是一个定位内存泄露的工具，适用于Android平台。

主要特性如下：

- 对系统负载低
- 不需要修改目标进程的源代码
- 支持Andoroid上的大多数native进程
- 对**函数调用栈**自动进行解析，大多情况下不需要找带符号表的程序／库反查地址
- 支持Android多数较新的版本(Android4.0及以上）
- 需要root权限支持



## 2、HeapSnap 工具使用

先让目标进程加载libehapsnap.so，然后再使用kill命令发信号给目标进程去保存heap。下面介绍三种加载libehapsnap.so动态库的方法

### 2.1 使用heapsnap工具加载动态库

该方法通过进程注入的方式把代码加载到目标进程内。

* 把heapsnap和libheapsnap.so推送到机器（程序也可以推送到其它目录下）
```shell
adb shell chmod 0777 /data/local/tmp
adb push libheapsnap/libheapsnap.so /data/local/tmp/libheapsnap.so
adb shell chmod 0644 /data/local/tmp/libheapsnap.so
adb push heapsnap /data/local/tmp/heapsnap
adb shell chmod 0755 /data/local/tmp/heapsnap
```
* 通过adb或者串口登陆目标机器，开启malloc调试，并重启目标进程
```shell
setprop libc.debug.malloc 1
setprop libc.debug.malloc.options backtrace
stop;start
[执行你的应用]
```
可参考文章[Android Libc Debug](https://albuer.github.io/2019/11/30/Android-libc-debug/)
* 加载动态库
```
/data/local/tmp/heapsnap -p <pid> -l /data/local/tmp/libheapsnap.so
```
* 通过signal 21获取目标进程的heap信息，并自动保存在/data/local/tmp/heap_snap目录下  
	```
	kill -21 [pid]
	```
* 多次在不同时间点获取目标进程的heap信息，并对这些heap信息进行比对，从而找出异常的内存分配  
* 也可以选择在加载动态库时直接执行动态库中的函数保存heap信息，然后马上关闭动态库．以后每次获取heap信息都需要调用相同的命令：  
```
/data/local/tmp/heapsnap -p <pid> -l /data/local/tmp/libheapsnap.so -o -f heapsnap_save
```
获取到的heap信息保存在： /data/local/tmp/heap_snap 目录下
对于已经加载库的进程，也可以这么调用获取heap信息．

### 2.2 LD_PRELOAD加载动态库
* 把libheapsnap.so推送到机器
```shell
adb shell chmod 0777 /data/local/tmp
adb push libheapsnap/libheapsnap.so /data/local/tmp/libheapsnap.so
adb shell chmod 0644 /data/local/tmp/libheapsnap.so
```

* 通过adb或者串口登陆目标机器，开启malloc调试，并重启目标进程(以mediaserver为例)
```shell
setprop libc.debug.malloc 1
stop media
LD_PRELOAD=/data/local/tmp/libheapsnap.so mediaserver &
```
可参考文章[Android Libc Debug](https://albuer.github.io/2019/11/30/Android-libc-debug/)

也可以配置当前shell的环境变量，避免每次都要在目标进程前加"LD_PRELOAD"前缀
```
export LD_PRELOAD=/data/local/tmp/libheapsnap.so
mediaserver &
```

* 通过signal 21获取目标进程的heap信息，并自动保存文件到/data/local/tmp/heap_snap/目录下
```
kill -21 [pid]
```

* 多次在不同时间点获取目标进程的heap信息，并对这些heap信息进行比对，从而找出异常的内存分配

### 2.3 目标程序编译时候链接动态库

在目标程序的编译脚本中加入下面这行，然后在你的程序中调用heapsnap_save()，重新编译好的程序在启动时候会自动链接libheapsnap.so库。

缺点就是需要目标程序的源代码及编译环境。

```
LOCAL_SHARED_LIBRARIES := libheapsnap
```

可以参考src/leak_builtin.c代码

## 3、解析backtrace

使用heapsnap获得的heap信息，已经自动对地址做了解析，如下所示：

```
Heap Snapshot v1.0

Total memory: 33800
Allocation records: 3
Backtrace size: 32

z 0  sz     4096  num    8  bt 0000007f9a2b0e14 0000007f9a37f534 00000055828338bc 000000558283379c 0000007f9a37f6b0 0000005582833810
          #00  pc 0000000000008e14  /system/lib64/libc_malloc_debug_leak.so (leak_malloc+408)
          #01  pc 0000000000019534  /system/lib64/libc.so (malloc+24)
          #02  pc 00000000000008bc  /system/bin/leak_test
          #03  pc 000000000000079c  /system/bin/leak_test (main+28)
          #04  pc 00000000000196b0  /system/lib64/libc.so (__libc_init+104)
          #05  pc 0000000000000810  /system/bin/leak_test
```

通常，heapsnap所解析出来的backtrace信息已经能够大致判断出泄露点了。

但是有时候你需要更精确的解析，如果你手上有设备所对应的android环境，那么可以使用android提供的工具进行backtrace地址解析

* android 6及早期版本
	```shell
	$ development/scripts/stack heap.txt > heap_info.txt
	```
* android7以后的版本
	```shell
	$ development/scripts/native_heapdump_viewer.py heap.txt > heap_info.txt
	```
	

解析后的信息如下：

```
    BYTES %TOTAL %PARENT    COUNT    ADDR LIBRARY FUNCTION LOCATION
        0   0.00%   0.00%        0 APP   

   107808 100.00% 100.00%      198 ZYGOTE   
   107800  99.99%  99.99%      197   5d1b2c0678 /system/bin/leak_test do_arm64_start external/heapsnap.git/leak_test.c:?
   107800  99.99% 100.00%      197     77cd006594 /system/lib64/libc.so __libc_init /proc/self/cwd/bionic/libc/bionic/libc_init_dynamic.cpp:109
    98304  91.18%  91.19%       24       5d1b2c073c /system/bin/leak_test foo /proc/self/cwd/external/heapsnap.git/leak_test.c:9 (discriminator 1)
     8472   7.86%   7.86%      172       5d1b2c0764 /system/bin/leak_test main /proc/self/cwd/external/heapsnap.git/leak_test.c:22
     8472   7.86% 100.00%      172         77cd02a084 /system/lib64/libc.so sleep /proc/self/cwd/bionic/libc/upstream-freebsd/lib/libc/gen/sleep.c:58
     8472   7.86% 100.00%      172           77cd05756c /system/lib64/libc.so nanosleep /proc/self/cwd/bionic/libc/arch-arm64/syscalls/nanosleep.S:7
     8472   7.86% 100.00%      172             77cd2206bc [vdso] ??? ???
...
```

**注意: 对于android7/8/9版本，需要打下面这个补丁，才能正常解析heapsnap保存下来的heap信息**

```shell
$ cd android/development
$ patch -p1 < android_7_8_9_development_script.patch
```



## 4、注意点

* 注意arm/arm64版本的区别，如果调试的目标程序是32bit，请使用arm版本的heapsnap程序和libheapsnap.so库；如果调试的目标程序是64bit，就要使用arm64版本的heapsnap程序和libheapsnap.so库。
* 对于android 9/10及以后的版本，若你调试的对象是**/vendor**目录下的程序，那么你必须把**libheapsnap.so**文件也放到/vendor目录下，否则加载libheapsnap.so文件会失败。
* 对于android 7/8/9版本，在这几个版本中把获得的heap信息，以及解析脚本"native_heapdump_viewer.py"存在问题，heapsnap对heap信息有做了修正；也提供了补丁"android_7_8_9_development_script.patch"修正"native_heapdump_viewer.py"脚本的问题。
* LD_PRELOAD环境变量只对当前的shell有效，如果进程是做为service由init启动，需要先stop该进程，然后在shell下启动进程．
* 一些具有AT_SECURE属性的进程或者环境，它们在link处理过程中会忽略掉LD_PRELOAD参数，即LD_PRELOAD对该类进程或环境不起作用．
* 使用heapsnap需要**root权限**．

