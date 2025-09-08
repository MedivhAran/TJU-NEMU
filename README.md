# ICS2021 Programming Assignment

This project is the programming assignment of the class ICS(Introduction to Computer System) in College of Intelligence and Computing, Tianjin Univerisity.

This project is introduced from Nanjin University in 2016. Thank you for  Prof. Chunfeng Yuan of NJU and Dr. Zihao Yu of ICT.

The following subprojects/components are included. Some of them are not fully implemented.
* NEMU
* testcase
* uClibc
* kernel
* typing game
* NEMU-PAL

## NEMU

NEMU(NJU Emulator) is a simple but complete full-system x86 emulator designed for teaching. It is the main part of this programming assignment. Small x86 programs can run under NEMU. The main features of NEMU include
* a small monitor with a simple debugger
 * single step
 * register/memory examination
 * expression evaluation with the support of symbols
 * watch point
 * backtrace
* CPU core with support of most common used x86 instructions in protected mode
 * real mode is not supported
 * x87 floating point instructions are not supported
* DRAM with row buffer and burst
* two-level unified cache
* IA-32 segmentation and paging with TLB
 * protection is not supported
* IA-32 interrupt and exception
 * protection is not supported
* 6 devices
 * timer, keyboard, VGA, serial, IDE, i8259 PIC
 * most of them are simplified and unprogrammable
* 2 types of I/O
 * port-mapped I/O and memory-mapped I/O

## testcase

Some small C programs to test the implementation of NEMU.

## uClibc

uClibc(https://www.uclibc.org/) is a C library for embedding systems. It requires much fewer run-time support than glibc and is very friendly to NEMU.

## kernel

This is the simplified version of Nanos(http://cslab.nju.edu.cn/opsystem). It is a uni-tasking kernel with the following features.
* 2 device drivers
 * Ramdisk
 * IDE
* ELF32 loader
* memory management with paging
* a simple file system
 * with fix number and size of files
 * without directory
* 6 system calls
 * open, read, write, lseek, close, brk

## typing game

This is a fork of the demo of NJU 2013 oslab0(the origin repository has been deleted, but we have a fork of it -- https://github.com/nju-ics/os-lab0). It is ported to NEMU.

## NEMU-PAL

This is a fork of Wei Mingzhi's SDLPAL(https://github.com/CecilHarvey/sdlpal). It is obtained by refactoring the original SDLPAL, as well as porting to NEMU.


本项目是天津大学智能与计算学部《ICS（计算机系统导论）》课程的编程作业。

该项目最早由南京大学在 2016 年引入。特别感谢南京大学袁春风教授和中国科学院计算所余子豪博士。

项目包含以下子项目/组件，其中部分尚未完整实现：

NEMU
testcase
uClibc
kernel
打字游戏
NEMU-PAL
NEMU
NEMU（NJU Emulator）是一个简洁但完整的全系统 x86 模拟器，主要用于教学。它是本次编程作业的核心部分，可以在其上运行小型 x86 程序。NEMU 的主要特性包括：

一个带有简单调试器的小型监视器
单步执行
寄存器/内存检查
支持符号的表达式求值
监视点
回溯
支持大多数常用保护模式 x86 指令的 CPU 内核
不支持实模式
不支持 x87 浮点指令
带有行缓冲和突发模式的 DRAM
两级统一缓存
带有 TLB 的 IA-32 段式分页机制

sudo service docker restart
sudo docker start nemu-image
sudo docker exec -it nemu-image /bin/bash 