import os
import ycm_core

def Settings( **kwargs ):
    return { 
        'flags': [
            "-xc", 
            "-I", "/home/seiran/.local/share/fxsdk/sysroot/bin/../lib/gcc/sh3eb-elf/14.1.0/include",
            "-I", "src/include", 
            "-I", "src", 
            "-I", "/home/seiran/.local/share/fxsdk/sysroot/sh3eb-elf/include", 

            "-D", "FXCG50", 
            "-Wall", "-Wextra",

            "-std=c23"
            ] 
    }
