# This is all a bit crap, but whatever. It's just less annoying to use docker images on MacOS + ARM
# than literally anything else for debugging, because x86 Vagrant + VBox isn't really a thing on ARM.
FROM debian:buster

WORKDIR /code
COPY . ./
RUN bash ./init_debian_buster.sh

ENTRYPOINT ["tail", "-f", "/dev/null"]
