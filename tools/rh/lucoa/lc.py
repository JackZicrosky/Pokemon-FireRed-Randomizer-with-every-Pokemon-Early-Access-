# Lucoa overworld helpers: palette, load/save text frames, zoomed comparison renders.
import os, sys
from PIL import Image, ImageDraw
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
OLD={'.':(0,0,0,0),'K':(48,36,48),'T':(66,60,78),'t':(104,96,120),'Y':(238,241,133),'g':(196,200,88),'h':(179,236,120),'c':(139,204,247),'S':(253,231,213),'s':(234,180,158),'p':(222,110,136),'P':(246,163,170),'W':(226,224,230),'O':(230,178,138),'o':(168,110,78),'D':(80,84,168),'G':(27,135,98),'V':(117,105,158)}
# new 15-colour palette (+ transparent). s->O merged (skin shade = horn light), t->V merged.
NEW={'.':(0,0,0,0),
 'K':(48,36,48),     # outline (hair, clothes, cap)
 'T':(66,60,78),     # black tank top / thigh-highs
 'V':(117,105,158),  # top highlight, shadow under the bill, purple eye
 'Y':(238,241,133),  # hair yellow
 'g':(196,200,88),   # hair shade
 'h':(179,236,120),  # hair green
 'c':(139,204,247),  # hair blue tips / shorts cuffs
 'S':(253,231,213),  # skin
 'O':(232,170,132),  # skin shade + horn
 'o':(150,88,70),    # skin outline + horn rings
 'p':(222,110,136),  # cap shade / blush / shoes
 'P':(246,163,170),  # cap pink
 'W':(226,224,230),  # bill / band / shoe soles
 'D':(80,84,168),    # jean shorts
 'G':(27,135,98),    # green eye
}
def load(path):
    frames=[];cur=None
    for line in open(path):
        line=line.rstrip('\n')
        if line.startswith('# frame'): cur=[];frames.append(cur)
        elif line and not line.startswith('#'): cur.append(list(line))
    return frames
def save(frames,path,head='# Lucoa frames\n'):
    with open(path,'w') as f:
        f.write(head)
        for i,fr in enumerate(frames):
            f.write('# frame %d\n'%i+'\n'.join(''.join(r) for r in fr)+'\n')
def render(fr,pal):
    h=len(fr);w=len(fr[0]);im=Image.new('RGBA',(w,h))
    for y,row in enumerate(fr):
        for x,ch in enumerate(row):
            c=pal[ch]; im.putpixel((x,y),c if len(c)==4 else c+(255,))
    return im
def leaf_frame(i,name='normal.png',fw=16,fh=32,pack='leaf'):
    s=Image.open(REPO+'/graphics/rh_player/%s/%s'%(pack,name)); im=s.convert('RGBA'); px=im.load()
    for y in range(s.height):
        for x in range(s.width):
            if s.getpixel((x,y))==0: px[x,y]=(0,0,0,0)
    return im.crop((i*fw,0,i*fw+fw,fh))
def grid(rows,out,z=10,y0=8,y1=32,bg=(120,176,96,255)):
    """rows: list of lists of RGBA 16x32 images"""
    n=max(len(r) for r in rows); fw=rows[0][0].width
    o=Image.new('RGBA',(n*(fw*z+8)+8,len(rows)*((y1-y0)*z+8)+8),bg)
    for j,r in enumerate(rows):
        for i,im in enumerate(r):
            b=im.crop((0,y0,im.width,y1)).resize((im.width*z,(y1-y0)*z),Image.NEAREST)
            o.paste(b,(8+i*(fw*z+8),8+j*((y1-y0)*z+8)),b)
    o.save(out)
