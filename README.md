# pintos
压缩包里面的内容是源代码文件

src_with_answer文件夹是可以过的代码

src是保留框架和一些逻辑细节，去除大段重点函数之后的代码包

涉及到很多环境配置和模拟器选择的问题，我都已经在src_with_answer和src之中对makefile等文件做了更改

具体去做的时候，先把压缩包里面文件在linux下解压，然后直接删掉源文件中的src，再将我提供的整个src复制到pintos文件

需要补全的文件只有vm/page.c和userprog/syscall.c

要实现的两个主要功能分别是虚拟页面的管理和mmap系统调用

最重要的逻辑是page fault的修改，检查一下是否需要分配一个新物理页面还是说访问到了没有分配虚拟页面的地址（真正的错误地址）

实现了page.c之中所有函数之后应该可以过90个左右测试

在所有需要实现代码的地方都标注了/*wirte your codes here and implement following functions*/

实现page.c之前请务必完全看懂frame.c和swap.c的逻辑，这两个文件本身也是需要自己实现的，但是其中涉及到“工程性”的代码较多，而知识思路性的代码较少，且为了更好的逻辑自洽，这里给出了代码
