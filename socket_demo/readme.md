# 第一次socket编程demo

>[叶公好C](https://www.zhihu.com/question/393401553/answer/1209152470)
>
>从前有个人，他叫叶公，很喜欢吹C，他天天吹C无所不能，操作系统都是C写的，大部分语言的编译器和解释器也是C写的，运行效率最高的也是C。他这样爱C，老板便让他去搞web开发、桌面开发。sys/socket.h在/usr/lib里探望，各种系统底层API伸到了电脑里。叶公一看搞这些开发，吓得失了魂似的，转身就抱住了Java与C#的大腿，惊恐万分。

## 运行方法
```bash
make
# 开两个终端，先运行server再运行client
./server.out
./client.out
```
**暂时不支持空格和UTF字符，空格需要用`writeUtf`等函数转化，就暂时不做了。**