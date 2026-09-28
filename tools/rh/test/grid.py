import sys
from PIL import Image
names=sys.argv[2:]; out=sys.argv[1]
ims=[Image.open(n) for n in names]
w,h=ims[0].size; cols=3; rows=(len(ims)+cols-1)//cols
g=Image.new('RGB',(w*cols,h*rows))
for i,im in enumerate(ims): g.paste(im,((i%cols)*w,(i//cols)*h))
g.resize((w*cols*2,h*rows*2),Image.NEAREST).save(out)
