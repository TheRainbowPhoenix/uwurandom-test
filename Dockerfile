FROM debian:stable-slim AS base-gpc

RUN apt-get update
RUN apt-get install curl python3-pil git python3 build-essential pkg-config -y

# Install the C/C++ SDK and compiler
RUN apt install cmake libusb-1.0-0-dev libsdl2-dev libudisks2-dev libglib2.0-dev libpng-dev libncurses5-dev -y
RUN apt install libmpfr-dev libmpc-dev libgmp-dev libppl-dev flex texinfo python-is-python3 clangd openssl -y

# Codespace
ENV USERNAME="dev"

RUN apt install sudo -y

RUN useradd -rm -d /home/$USERNAME -s /bin/bash -g root -G sudo -u 1001 -p "$(openssl passwd -1 ${USERNAME})" $USERNAME
# RUN useradd -m -s /bin/bash -G sudo -u 1001 $USERNAME
RUN echo "dev ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

USER $USERNAME
WORKDIR /home/$USERNAME

ENV PATH="/home/$USERNAME/.local/bin:$PATH"
RUN mkdir /home/$USERNAME/.local/
RUN mkdir /home/$USERNAME/.local/bin

RUN mkdir /tmp/giteapc-install
WORKDIR /tmp/giteapc-install

RUN curl "https://git.planet-casio.com/Lephenixnoir/GiteaPC/archive/master.tar.gz" -o giteapc-master.tar.gz
RUN tar -xzf giteapc-master.tar.gz
RUN cp -r /tmp/giteapc-install/giteapc /home/$USERNAME/ 
WORKDIR /home/$USERNAME/giteapc
RUN python3 giteapc.py install Lephenixnoir/GiteaPC -y

# sysroot is part of fxsdk so this is needed first
RUN python3 giteapc.py install Lephenixnoir/fxsdk@dev -y
RUN python3 giteapc.py install Lephenixnoir/sh-elf-binutils:clean -y
RUN python3 giteapc.py install Lephenixnoir/sh-elf-gcc:clean -y
RUN python3 giteapc.py install Lephenixnoir/sh-elf-gdb -y

RUN python3 giteapc.py install Lephenixnoir/OpenLibm -y
RUN python3 giteapc.py install Vhex-Kernel-Core/fxlibc@dev -y
RUN python3 giteapc.py install Lephenixnoir/sh-elf-gcc -y  # again for any rebuild/update
RUN python3 giteapc.py install lda/gint-test@serial-toying -y
RUN python3 giteapc.py install Lephenixnoir/JustUI@dev -y

# USER $USERNAME
# WORKDIR /home/$USERNAME

# RUN echo "export SDK_DIR=${SDK_DIR}" >> ~/.bashrc
# RUN echo "export OLD_SDK_DIR=${OLD_SDK_DIR}" >> ~/.bashrc

WORKDIR /workspace
# RUN fxsdk new my-addin
# RUN cd my-addin
# RUN fxsdk build-cp
