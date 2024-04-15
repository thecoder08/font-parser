unsigned short ntohs(unsigned short in) {
    return ((in & 0xff00) >> 8) | ((in & 0x00ff) << 8);
}

unsigned int ntohl(unsigned int in) {
    return ((in & 0xff000000) >> 24) | ((in & 0x00ff0000) >> 8) | ((in & 0x0000ff00) << 8) | ((in & 0x000000ff) << 24);
}