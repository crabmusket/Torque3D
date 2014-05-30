apt-get update -y > /dev/null

# Install prerequisites
apt-get install \
	git \
	cmake=2.8.7-0ubuntu5 \
	build-essential=11.5ubuntu2.1 \
	nasm=2.09.10-1 \
	php5-cli=5.3.10-1ubuntu3.11 \
	libsdl-dev \
	libogg-dev \
	-y
