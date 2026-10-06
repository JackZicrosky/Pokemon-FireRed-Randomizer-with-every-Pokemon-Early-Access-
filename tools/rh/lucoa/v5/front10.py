# Front/oak pic = the owner's HQ "Lucoa Pixel Art 1" (120x120 native, 12px cells) reduced 2:1
# with colours picked from the reference's own palette (no blending), then the eye restored.
from PIL import Image
from hqpal import HP, INV
import half
im=half.half(0,0,1.6); p=im.load()
EDITS={(12,14):'1',(13,14):'2'}          # open green eye (iris lost in the reduction)
for (x,y),c in EDITS.items(): p[x,y]=HP[c]+(255,)
bb=im.getbbox(); fig=im.crop(bb); print('figure',fig.size)
front=Image.new('RGBA',(64,64),(0,0,0,0))
front.paste(fig,((64-fig.width)//2,64-fig.height),fig)
front.save('front10.png')
oak=Image.new('RGBA',(64,96),(0,0,0,0)); oak.paste(front,(0,32),front); oak.save('oak10.png')
with open('front10.txt','w') as f:
    for y in range(64): f.write(''.join('.' if front.getpixel((x,y))[3]==0 else INV[front.getpixel((x,y))[:3]] for x in range(64))+'\n')
print('colours',len({c for c in front.getdata() if c[3]}))
# preview: reference native | front 64x64 at 2x scale-equivalent
ref=Image.open('hq_native.png').crop((16,0,96,120)).resize((80*4,120*4),Image.NEAREST)
def onbg(i,z,bg=(0,97,82,255)):
    b=Image.new('RGBA',i.size,bg); b.alpha_composite(i); return b.resize((i.width*z,i.height*z),Image.NEAREST)
f8=onbg(front,8)
s=Image.new('RGB',(ref.width+f8.width+30,max(ref.height,f8.height)),(30,30,30))
s.paste(ref,(0,0)); s.paste(f8.convert('RGB'),(ref.width+30,(s.height-f8.height)//2)); s.save('cmp10.png')
onbg(front,6).save('front10_x6.png'); onbg(front,1).save('front10_1x.png')
