FROM python:2.7.10

RUN apt-get update -y
RUN apt-get upgrade -y
RUN apt-get install -y libperl-dev \
                        libgtk2.0-dev \
                        libglib2.0-dev \
                        libfdt-dev \
                        libpixman-1-dev \
                        zlib1g-dev
RUN git clone https://github.com/phonyphonecall/qemu.git
RUN cd qemu && ./configure --target-list="microblaze-softmmu" && make 

