clear

printf '\n\n\n\n\n'
# ffmpeg -i 5EB3C0334770414A6AD.wav -f s16be -ar 1600 -acodec pcm_s16be file.pcm

RED='\033[1;44;31m'
NC='\033[0m'

# common
# gcc -o ExtractData ZTScore.c &&  echo -e "${RED}compile done${NC}" && printf '\n\n\n' && ./ExtractData # 1>log 2>&1

# mac
# brew install clang & brew install llvm & brew install libomp
# /usr/local/opt/llvm/bin/clang ZTScore.c -fopenmp -o ExtractData  &&  echo -e "${RED}compile done${NC}" && printf '\n\n\n' && ./ExtractData


# ubuntu
# sudo apt-get install clang
# sudo apt install libomp-dev
clang ZTScore.c -fopenmp -lm -o ExtractData  &&  echo -e "${RED}compile done${NC}" && printf '\n\n\n' && ./ExtractData
