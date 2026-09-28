import sys, zlib, hashlib
def vlq(d,i):
    r=0;s=1
    while True:
        x=d[i];i+=1;r+=(x&0x7f)*s
        if x&0x80: return r,i
        s<<=7;r+=s
src=open(sys.argv[1],'rb').read(); p=open(sys.argv[2],'rb').read()
assert p[:4]==b'BPS1'
sc,tc,pc=[int.from_bytes(p[-12+4*k:-8+4*k] if k<2 else p[-4:],'little') for k in range(3)]
assert zlib.crc32(p[:-4])==pc, 'patch crc bad'
assert zlib.crc32(src)==sc, 'source crc mismatch'
i=4; ss,i=vlq(p,i); ts,i=vlq(p,i); ms,i=vlq(p,i); i+=ms
out=bytearray(ts); o=0; so=0; to=0; end=len(p)-12
while i<end:
    d,i=vlq(p,i); a=d&3; n=(d>>2)+1
    if a==0: out[o:o+n]=src[o:o+n]; o+=n
    elif a==1: out[o:o+n]=p[i:i+n]; i+=n; o+=n
    else:
        v,i=vlq(p,i); v=(-1 if v&1 else 1)*(v>>1)
        if a==2:
            so+=v; out[o:o+n]=src[so:so+n]; so+=n; o+=n
        else:
            to+=v
            for _ in range(n): out[o]=out[to]; o+=1; to+=1
assert zlib.crc32(out)==tc, 'target crc mismatch'
print('OK', sys.argv[2].split('/')[-1], 'size', ts, 'sha1', hashlib.sha1(out).hexdigest())
