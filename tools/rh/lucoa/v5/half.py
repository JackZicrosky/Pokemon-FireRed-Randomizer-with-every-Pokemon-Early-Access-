import sys
from PIL import Image
from collections import Counter
BG=(0,97,82)
nat=Image.open('hq_native.png'); q=nat.load()
def lum(c): return 0.3*c[0]+0.59*c[1]+0.11*c[2]
def half(ox,oy,dark):
    W=(54+ox+1)//2+1; H=(120+oy+1)//2+1
    out=Image.new('RGBA',(W,H),(0,0,0,0)); o=out.load()
    for by in range(H):
        for bx in range(W):
            c=Counter()
            for j in range(2):
                for i in range(2):
                    x=28+bx*2+i-ox; y=by*2+j-oy
                    if 0<=x<120 and 0<=y<120:
                        col=q[x,y]
                        w=1.0
                        if col!=BG and lum(col)<70: w=dark
                        if col==BG: w=0.9
                        c[col]+=w
            if not c: continue
            col=c.most_common(1)[0][0]
            if col!=BG: o[bx,by]=col+(255,)
    return out
if __name__=='__main__':
    tiles=[]
    for ox in (0,1):
        for oy in (0,1):
            for dark in (1.0,1.6):
                im=half(ox,oy,dark); im.save(f'half_{ox}{oy}_{dark}.png'); tiles.append(im)
    W=max(t.width for t in tiles); H=max(t.height for t in tiles)
    sheet=Image.new('RGBA',((W+2)*8*6,(H+2)*6),(0,97,82,255))
    for k,t in enumerate(tiles):
        sheet.paste(t.resize((t.width*6,t.height*6),Image.NEAREST),(k*(W+2)*6,0),t.resize((t.width*6,t.height*6),Image.NEAREST))
    sheet.save('half_sheet.png')
