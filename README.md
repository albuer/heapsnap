# HeapSnap: Android进程堆内存泄露追踪工具

## 1、HeapSnap 是什么

[HeapSnap](https://github.com/albuer/heapsnap) 是一款针对Android进程堆内存进行追踪、定位，以便查出泄露位置的工具。主要特性如下：

- 对系统负载低
- 不需要修改目标进程的源代码
- 支持Andoroid上的大多数native进程
- 对**函数调用栈**自动进行解析，大多情况下不需要找带符号表的程序／库反查地址
- 支持Android多数较新的版本(Android4.0及以上）
- 需要root权限支持

## 2、HeapSnap 如何使用

### 2.1 使用工具加载动态库
* 把heapsnap和libheapsnap.so推送到机器
```shell
adb push libheapsnap/libheapsnap.so /data/local/tmp/libheapsnap.so
adb shell chmod 0644 /data/local/tmp/libheapsnap.so
adb push heapsnap /data/local/tmp/heapsnap
adb shell chmod 0755 /data/local/tmp/heapsnap
adb shell mkdir -p /data/local/tmp/heap_snap
adb shell chmod 0777 /data/local/tmp/heap_snap
```
* 通过adb或者串口登陆目标机器，开启malloc调试，并重启目标进程
```shell
setprop libc.debug.malloc 1
setprop libc.debug.malloc.options backtrace
stop;start
[执行你的应用]
```
* 加载动态库
```
/data/local/tmp/heapsnap -p <pid> -l /data/local/tmp/libheapsnap.so
```
* 通过signal 21获取目标进程的heap信息，并自动保存在/data/local/tmp/heap_snap/目录下  
`kill -21 [pid]`
* 多次在不同时间点获取目标进程的heap信息，并对这些heap信息进行比对，从而找出异常的内存分配  
* 也可以选择在加载动态库时直接执行动态库中的函数保存heap信息，然后马上关闭动态库．以后每次获取heap信息都需要调用相同的命令：  
```
/data/local/tmp/heapsnap -p <pid> -l /data/local/tmp/libheapsnap.so -o -f heapsnap_save
```
获取到的heap信息保存在： /data/local/tmp/heap_snap/ 目录下
对于已经加载库的进程，也可以这么调用获取heap信息．

### 2.2 LD_PRELOAD加载动态库
* 把libheapsnap.so推送到机器
```shell
adb push libheapsnap/libheapsnap.so /data/local/tmp/libheapsnap.so
adb shell chmod 0644 /data/local/tmp/libheapsnap.so
adb shell mkdir -p /data/local/tmp/heap_snap
adb shell chmod 0777 /data/local/tmp/heap_snap
```

* 通过adb或者串口登陆目标机器，开启malloc调试，并重启目标进程(以mediaserver为例)
```shell
setprop libc.debug.malloc 1
stop media
LD_PRELOAD=/data/local/tmp/libheapsnap.so mediaserver &
```
也可以配置当前shell的环境变量，避免每次都要在目标进程前加"LD_PRELOAD"前缀
```
export LD_PRELOAD=/data/local/tmp/libheapsnap.so
mediaserver &
```

* 通过signal 21获取目标进程的heap信息，并自动保存文件到/data/local/tmp/heap_snap/目录下
`kill -21 [pid]`

* 多次在不同时间点获取目标进程的heap信息，并对这些heap信息进行比对，从而找出异常的内存分配

### 2.3 获取到的heap信息格式
```
...
size   404256, dup    1, 0xb6bff76e, 0xb6ecd282, 0xb577e1d8, 0xb577c9d8, 0xb577da24, 0xb5780338, 0xb57809f4, 0xb577bc08, 0xb58f9b00, 0xb58e8538, 0xb6ddf596, 0xb6e7b2a2, 0xb6e6aaf6, 0xb6f67b54, 0xb6ecd39c, 0xb6f67c74
          #00  pc 0000676e  /system/lib/libc_malloc_debug_leak.so (leak_malloc+101)
          #01  pc 00012282  /system/lib/libc.so (malloc+9)
          #02  pc 002631d8  /system/vendor/lib/egl/libGLES_mali.so
          #03  pc 002619d8  /system/vendor/lib/egl/libGLES_mali.so
          #04  pc 00262a24  /system/vendor/lib/egl/libGLES_mali.so
          #05  pc 00265338  /system/vendor/lib/egl/libGLES_mali.so
          #06  pc 002659f4  /system/vendor/lib/egl/libGLES_mali.so
          #07  pc 00260c08  /system/vendor/lib/egl/libGLES_mali.so
          #08  pc 003deb00  /system/vendor/lib/egl/libGLES_mali.so
          #09  pc 003cd538  /system/vendor/lib/egl/libGLES_mali.so (eglCreateContext+1184)
          #10  pc 00012596  /system/lib/libEGL.so (eglCreateContext+101)
          #11  pc 000292a2  /system/lib/libsurfaceflinger.so
          #12  pc 00018af6  /system/lib/libsurfaceflinger.so (android::SurfaceFlinger::init()+157)
          #13  pc 00000b54  /system/bin/surfaceflinger
          #14  pc 0001239c  /system/lib/libc.so (__libc_init+43)
          #15  pc 00000c74  /system/bin/surfaceflinger
...
```

### 2.4 注意点
* LD_PRELOAD环境变量只对当前的shell有效，如果进程是做为service由init启动，需要先stop该进程，然后在shell下启动进程．
* 一些具有AT_SECURE属性的进程或者环境，它们在link处理过程中会忽略掉LD_PRELOAD参数，即LD_PRELOAD对该类进程或环境不起作用．
* 使用heapsnap需要root权限．

