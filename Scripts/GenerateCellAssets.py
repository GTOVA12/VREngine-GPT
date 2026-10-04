"""Generate the detailed product cell meshes and PBR textures deterministically.
All geometry and texture pixels are authored here; no downloaded mesh assets.
"""
import math
import struct
import json
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "Assets"
MODELS = ROOT / "Models"
TEXTURES = ROOT / "Textures"
MODELS.mkdir(parents=True, exist_ok=True)
TEXTURES.mkdir(parents=True, exist_ok=True)

def png(name, width, height, pixel):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x, y))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    (TEXTURES / name).write_bytes(data)

def grain(x, y):
    value = ((x * 73856093) ^ (y * 19349663)) & 255
    return value / 255.0

png("CellMetal.png", 512, 512, lambda x, y: tuple(int(210 + 9*grain(x,y) + 3*math.sin(y*.7)) for _ in range(3)))
png("CellRubber.png", 512, 512, lambda x, y: (int(37 + 4*grain(x,y) + (6 if x%32<2 else 0)),)*3)
png("CellNormal.png", 512, 512, lambda x, y: (128, int(128 + 3*math.sin(y*1.9)), 255))
png("CellFloor.png", 512, 512, lambda x,y: (110,112,114) if x<1 or y<1 else (int(137+grain(x,y)*3),int(139+grain(x,y)*3),int(141+grain(x,y)*3)))

FONT = {
"A":["01110","10001","10001","11111","10001","10001","10001"],
"B":["11110","10001","10001","11110","10001","10001","11110"],
"C":["01111","10000","10000","10000","10000","10000","01111"],
"D":["11110","10001","10001","10001","10001","10001","11110"],
"E":["11111","10000","10000","11110","10000","10000","11111"],
"F":["11111","10000","10000","11110","10000","10000","10000"],
"I":["111","010","010","010","010","010","111"],
"L":["10000","10000","10000","10000","10000","10000","11111"],
"N":["10001","11001","11001","10101","10011","10011","10001"],
"O":["01110","10001","10001","10001","10001","10001","01110"],
"P":["11110","10001","10001","11110","10000","10000","10000"],
"R":["11110","10001","10001","11110","10100","10010","10001"],
"S":["01111","10000","10000","01110","00001","00001","11110"],
"T":["11111","00100","00100","00100","00100","00100","00100"],
"U":["10001","10001","10001","10001","10001","10001","01110"],
"Y":["10001","10001","01010","00100","00100","00100","00100"],
"0":["01110","10001","10011","10101","11001","10001","01110"],
"1":["00100","01100","00100","00100","00100","00100","01110"],
"-":["00000","00000","00000","11111","00000","00000","00000"],
" ":["000"]*7
}

def panel(name, label, background, foreground, screen=False):
    width, height = 512, 256
    pixels = [[list(background) for _ in range(width)] for _ in range(height)]
    def rect(x0,y0,w,h,color):
        for y in range(max(0,y0),min(height,y0+h)):
            for x in range(max(0,x0),min(width,x0+w)):
                pixels[y][x] = list(color)
    def text(value,x,y,scale,color):
        for ch in value:
            glyph = FONT.get(ch,FONT[" "])
            for row, bits in enumerate(glyph):
                for col, bit in enumerate(bits):
                    if bit=="1": rect(x+col*scale,y+row*scale,scale,scale,color)
            x += (len(glyph[0])+1)*scale
    if screen:
        rect(12,12,488,36,(28,57,69))
        text("CELL 01",25,23,3,foreground)
        text("PLC READY",25,73,4,(92,218,189))
        for i in range(3):
            rect(24,125+i*31,35,20,(55,166,140))
            rect(76,125+i*31,280-i*48,12,(51,73,85))
        rect(410,75,65,122,(22,48,62))
        for i in range(6): rect(418,87+i*15,49,3,(94,172,191))
    else:
        text(label,24,91,7,foreground)
        rect(24,185,464,3,foreground)
        text("FACTORYCORE",24,215,3,foreground)
    png(name,width,height,lambda x,y: pixels[y][x])

panel("CellPanel.png","",(10,24,34),(201,223,230),True)
panel("CellLabel.png","CELL 01",(230,231,226),(26,49,59))
panel("ProductLabel.png","FC-01",(225,224,204),(57,66,62))

materials = [
    {"name":"Satin machined aluminium","pbrMetallicRoughness":{"baseColorFactor":[.72,.76,.79,1],"metallicFactor":.88,"roughnessFactor":.25,"baseColorTexture":{"index":0}},"normalTexture":{"index":2,"scale":.35}},
    {"name":"Brushed stainless steel","pbrMetallicRoughness":{"baseColorFactor":[.72,.76,.81,1],"metallicFactor":.96,"roughnessFactor":.19}},
    {"name":"Graphite powder coat","pbrMetallicRoughness":{"baseColorFactor":[.035,.047,.060,1],"metallicFactor":.28,"roughnessFactor":.44}},
    {"name":"Deep teal enclosure","pbrMetallicRoughness":{"baseColorFactor":[.035,.22,.245,1],"metallicFactor":.3,"roughnessFactor":.31}},
    {"name":"Safety yellow","pbrMetallicRoughness":{"baseColorFactor":[.95,.57,.035,1],"metallicFactor":.08,"roughnessFactor":.37}},
    {"name":"Textured conveyor rubber","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":0,"roughnessFactor":.79,"baseColorTexture":{"index":1}}},
    {"name":"Product ivory polymer","pbrMetallicRoughness":{"baseColorFactor":[.76,.75,.64,1],"metallicFactor":.04,"roughnessFactor":.36}},
    {"name":"Status green","pbrMetallicRoughness":{"baseColorFactor":[.04,.7,.24,1],"metallicFactor":0,"roughnessFactor":.22},"emissiveFactor":[.02,.35,.08]},
    {"name":"Alarm red","pbrMetallicRoughness":{"baseColorFactor":[.8,.018,.015,1],"metallicFactor":0,"roughnessFactor":.3},"emissiveFactor":[.3,.003,.002]},
    {"name":"Protective polycarbonate","pbrMetallicRoughness":{"baseColorFactor":[.50,.75,.84,.16],"metallicFactor":0,"roughnessFactor":.08},"alphaMode":"BLEND","doubleSided":True},
    {"name":"Operator display","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":0,"roughnessFactor":.31,"baseColorTexture":{"index":4}},"emissiveTexture":{"index":4},"emissiveFactor":[.3,.3,.3]},
    {"name":"Identification plate","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":.05,"roughnessFactor":.45,"baseColorTexture":{"index":5}}},
    {"name":"Product identification","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":0,"roughnessFactor":.6,"baseColorTexture":{"index":6}}},
    {"name":"Workshop concrete","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":.03,"roughnessFactor":.72,"baseColorTexture":{"index":3}}},
    {"name":"Blue pneumatic tube","pbrMetallicRoughness":{"baseColorFactor":[.025,.24,.55,1],"metallicFactor":.02,"roughnessFactor":.34}},
]

def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a):
    length=math.sqrt(dot(a,a))
    return tuple(v/length for v in a)

def box(size):
    half=[s/2 for s in size]; radius=min(size)*.10
    inner=[s-radius for s in half]
    faces=[(0,1,2,1),(0,2,1,-1),(1,2,0,1),(1,0,2,-1),(2,0,1,1),(2,1,0,-1)]
    positions=[]; normals=[]; uv=[]; indices=[]
    for axis, u, v, sign in faces:
        def samples(k):
            h=half[k]; q=inner[k]
            return [-h,-q-radius*.707,-q,0,q,q+radius*.707,h]
        us, vs = samples(u),samples(v); base=len(positions)
        for j,b in enumerate(vs):
            for i,a in enumerate(us):
                p=[0.0]*3; p[axis]=sign*half[axis]; p[u]=a; p[v]=b
                center=[max(-inner[k],min(inner[k],p[k])) for k in range(3)]
                normal=norm([p[k]-center[k] for k in range(3)])
                positions.append(tuple(center[k]+radius*normal[k] for k in range(3)))
                normals.append(normal); uv.append((p[u]/size[u]+.5,.5-p[v]/size[v]))
        for j in range(6):
            for i in range(6):
                a=base+j*7+i; b=a+1; c=a+8; d=a+7
                indices.extend([a,b,c,a,c,d])
    return positions,normals,uv,indices

def cylinder(size,segments=64):
    length,dy,dz=size; radius=dy/2; bevel=min(length,dy,dz)*.10
    positions=[]; normals=[]; uv=[]; indices=[]
    rings=[(-length/2,radius-bevel,-1,0),(-length/2+bevel*.293,radius-bevel*.293,-.707,.707),
           (-length/2+bevel,radius,0,1),(length/2-bevel,radius,0,1),
           (length/2-bevel*.293,radius-bevel*.293,.707,.707),(length/2,radius-bevel,1,0)]
    for x,r,nx,nr in rings:
        for j in range(segments+1):
            t=j*math.tau/segments
            positions.append((x,r*math.cos(t),r*math.sin(t)*dz/dy))
            normals.append(norm((nx,nr*math.cos(t),nr*math.sin(t)*dy/dz)) if nr else (nx,0,0))
            uv.append((j/segments,(x+length/2)/length))
    for ring in range(len(rings)-1):
        for j in range(segments):
            a=ring*(segments+1)+j; b=a+1; c=b+segments+1; d=a+segments+1
            indices.extend([a,b,c,a,c,d])
    for sign in [-1,1]:
        base=len(positions); positions.append((sign*length/2,0,0)); normals.append((sign,0,0)); uv.append((.5,.5))
        for j in range(segments+1):
            t=j*math.tau/segments
            positions.append((sign*length/2,(radius-bevel)*math.cos(t),(radius-bevel)*math.sin(t)*dz/dy))
            normals.append((sign,0,0)); uv.append((.5+.5*math.cos(t),.5-.5*math.sin(t)))
        for j in range(segments):
            indices.extend([base,base+j+1,base+j+2] if sign>0 else [base,base+j+2,base+j+1])
    return positions,normals,uv,indices

class Model:
    def __init__(self,name):
        self.name=name; self.nodes=[]; self.meshes=[]; self.data=bytearray(); self.views=[]; self.accessors=[]
        self.cache={}
    def group(self,name,pos=(0,0,0)):
        self.nodes.append({"name":name,"translation":pos,"children":[]})
        return len(self.nodes)-1
    def accessor(self,values,kind,fmt,gltype,target):
        while len(self.data)%4:self.data.append(0)
        flat=values if kind=="SCALAR" else [v for row in values for v in row]
        start=len(self.data); self.data.extend(struct.pack("<"+fmt*len(flat),*flat))
        self.views.append({"buffer":0,"byteOffset":start,"byteLength":len(self.data)-start,"target":target})
        item={"bufferView":len(self.views)-1,"componentType":gltype,"count":len(values),"type":kind}
        if kind=="VEC3":
            item["min"]=[min(v[k] for v in values) for k in range(3)]
            item["max"]=[max(v[k] for v in values) for k in range(3)]
        self.accessors.append(item);return len(self.accessors)-1
    def add(self,name,size,pos,material,shape="box",parent=None,axis="x"):
        key=(tuple(size),material,shape)
        if key not in self.cache:
            p,n,u,idx=box(size) if shape=="box" else cylinder(size)
            if self.name=="CellFloor": u=[(a*80,b*80) for a,b in u]
            attrs={"POSITION":self.accessor(p,"VEC3","f",5126,34962),"NORMAL":self.accessor(n,"VEC3","f",5126,34962),"TEXCOORD_0":self.accessor(u,"VEC2","f",5126,34962)}
            indices=self.accessor(idx,"SCALAR","I",5125,34963)
            self.meshes.append({"name":name,"primitives":[{"attributes":attrs,"indices":indices,"material":material}]})
            self.cache[key]=len(self.meshes)-1
        item={"name":name,"mesh":self.cache[key],"translation":pos}
        if axis=="y":item["rotation"]=[0,0,math.sqrt(.5),math.sqrt(.5)]
        if axis=="z":item["rotation"]=[0,-math.sqrt(.5),0,math.sqrt(.5)]
        self.nodes.append(item); result=len(self.nodes)-1
        if parent is not None:self.nodes[parent]["children"].append(result)
        return result
    def pipe(self,name,points,radius,material):
        for a,b in zip(points,points[1:]):
            d=[b[k]-a[k] for k in range(3)]; length=math.sqrt(dot(d,d)); x=norm(d)
            q=cross((1,0,0),x)
            w=math.sqrt(max(0,(1+x[0])/2))
            quat=[0,0,1,0] if w<1e-7 else [v/(2*w) for v in q]+[w]
            index=self.add(name,(length,radius*2,radius*2),tuple((a[k]+b[k])/2 for k in range(3)),material,"cylinder")
            self.nodes[index]["rotation"]=quat
    def bolts(self,points):
        for p in points:self.add("Hex fastener",(.010,.018,.018),p,1,"cylinder",axis="z")
    def save(self):
        child={i for n in self.nodes for i in n.get("children",[])}
        images=["CellMetal.png","CellRubber.png","CellNormal.png","CellFloor.png","CellPanel.png","CellLabel.png","ProductLabel.png"]
        doc={"asset":{"version":"2.0","generator":"FactoryCore detailed cell asset generator"},"scene":0,"scenes":[{"nodes":[i for i in range(len(self.nodes)) if i not in child]}],
             "nodes":self.nodes,"meshes":self.meshes,"materials":materials,
             "images":[{"uri":"../Textures/"+f} for f in images],"textures":[{"source":i,"sampler":0} for i in range(len(images))],
             "samplers":[{"magFilter":9729,"minFilter":9987,"wrapS":10497,"wrapT":10497}],
             "buffers":[{"uri":self.name+".bin","byteLength":len(self.data)}],"bufferViews":self.views,"accessors":self.accessors}
        (MODELS/(self.name+".bin")).write_bytes(self.data)
        (MODELS/(self.name+".gltf")).write_text(json.dumps(doc,indent=2)+"\n",encoding="utf-8",newline="\n")
        print(self.name,len(self.nodes),"parts",len(self.data),"bytes")

m=Model("CellConveyor")
m.add("Continuous belt",(3.48,.045,.58),(0,.777,0),5)
for z in [-.36,.36]:
    m.add("Extruded side rail",(3.6,.17,.065),(0,.665,z),0)
    m.add("Guide rail",(3.40,.055,.023),(0,.845,z*.87),1)
    m.add("Rail channel",(3.42,.012,.007),(0,.68,z+( -.035 if z<0 else .035)),2)
    for x in [-1.5,-.8,0,.8,1.5]:
        m.add("Belt guide bracket",(.035,.10,.06),(x,.76,z),2)
        m.bolts([(x,.68,z+(.04 if z>0 else -.04))])
for x in [-1.63,1.63]:
    m.add("BeltRoller",(.64,.11,.11),(x,.725,0),1,"cylinder",axis="z")
    for z in [-.39,.39]:
        m.add("Bearing block",(.15,.12,.07),(x,.69,z),2)
        m.add("Bearing cover",(.025,.06,.06),(x,.69,z+(.045 if z>0 else -.045)),1,"cylinder",axis="z")
for x in [-1.35,1.35]:
    for z in [-.32,.32]:
        m.add("Vertical extrusion",(.055,.58,.055),(x,.325,z),0)
        m.add("Levelling foot",(.03,.105,.105),(x,.015,z),2,"cylinder",axis="y")
        m.add("Foot stud",(.10,.027,.027),(x,.08,z),1,"cylinder",axis="y")
    m.add("Cross brace",(.055,.05,.65),(x,.19,0),0)
for z in [-.32,.32]:m.add("Lower stretcher",(2.7,.04,.04),(0,.19,z),0)
for z in [-.52,.52]:
    m.add("Gantry column",(.075,1.95,.075),(0,.99,z),0)
    m.add("Gantry foot",(.28,.045,.22),(0,.0225,z),2)
    m.add("Column slot",(.012,1.72,.009),(.042,1.1,z),2)
m.add("Gantry upper beam",(.16,.12,1.12),(0,1.975,0),0)
m.add("Head mounting plate",(.28,.06,.31),(0,1.845,0),1)
m.add("Rear safety panel",(1.0,.75,.009),(0,1.31,-.53),9)
for x in [-.5,.5]:m.add("Guard edge",(.018,.77,.018),(x,1.31,-.53),4)
for y in [.93,1.69]:m.add("Guard edge",(1.02,.02,.02),(0,y,-.53),4)
m.add("PLC cabinet",(.57,.74,.24),(-.67,.64,.78),3)
m.add("Cabinet door",(.52,.66,.018),(-.67,.64,.912),2)
m.add("HMI bezel",(.31,.19,.021),(-.67,.80,.931),0)
m.add("PLC operator screen",(.277,.145,.003),(-.67,.80,.945),10)
m.add("Cabinet label",(.29,.07,.003),(-.67,.48,.927),11)
for x in [-.83,-.68,-.53]:
    m.add("Pushbutton collar",(.022,.042,.042),(x,.62,.938),1,"cylinder",axis="z")
    m.add("Pushbutton",(.012,.028,.028),(x,.62,.956),7 if x<-.7 else 8 if x>-.6 else 4,"cylinder",axis="z")
m.add("Door handle",(.018,.17,.018),(-.45,.57,.939),1)
for x in [-.89,-.45]:
    m.add("Cabinet foot",(.075,.015,.075),(x,.0075,.77),2)
    m.add("Cabinet support",(.034,.20,.034),(x,.115,.77),0)
for z in [-.21,.21]:m.add("Infeed nose",(.23,.045,.04),(-1.79,.75,z),1)
for x in [-1.53,-1.47]:
    m.add("Yellow end marker",(.018,.012,.57),(x,.81,0),4)
for x in [-1.05,-.6,.45,.9,1.25]:
    m.add("Belt seam",(.007,.002,.575),(x,.802,0),2)
m.add("Motor support shelf",(.35,.04,.36),(1.60,.55,.52),1)
m.add("Motor support web",(.08,.14,.04),(1.63,.61,.36),1)
m.pipe("Cable harness",[(-.67,.27,.75),(-.67,.23,.47),(.85,.23,.47),(1.47,.57,.57)],.018,2)
m.pipe("Pneumatic supply",[(.03,1.90,.36),(.16,1.90,.36),(.16,1.70,.08),(.068,1.70,0)],.009,14)
m.save()

m=Model("CellMotor")
m.add("Motor housing",(.34,.20,.20),(.1,0,0),3,"cylinder")
m.add("Gearbox",(.14,.24,.24),(-.12,0,0),0)
m.add("Shaft",(.14,.05,.05),(-.25,0,0),1,"cylinder")
for x in [0,.05,.1,.15,.2]:
    m.add("Cooling rib",(.012,.215,.215),(x,0,0),2,"cylinder")
m.add("End cap",(.025,.19,.19),(.28,0,0),2,"cylinder")
m.add("Terminal box",(.12,.075,.11),(.11,.132,0),2)
m.add("Motor mounting",(.29,.035,.29),(.04,-.12,0),1)
m.add("Motion",(.04,.058,.04),(-.30,0,0),4)
m.pipe("Motor cable",[(.12,.15,0),(.16,.18,.08),(.33,.12,.11)],.012,2)
m.save()

m=Model("CellSensor")
m.add("Mount bracket",(.075,.12,.027),(0,-.02,0),0)
m.add("Photoelectric sensor",(.035,.050,.064),(0,.06,-.018),4)
m.add("Optical window",(.025,.033,.006),(0,.06,-.053),2)
m.add("Lens",(.005,.018,.018),(0,.06,-.058),8,"cylinder",axis="z")
m.add("Indicator",(.012,.006,.012),(0,.088,-.01),7)
m.pipe("Sensor cable",[(.015,.045,.02),(.022,-.06,.05),(.02,-.13,.08)],.005,2)
m.bolts([(0,-.035,.018)])
m.save()

m=Model("CellPress")
m.add("Pneumatic barrel",(.32,.105,.105),(.0,0,0),0)
for x in [-.18,.18]:
    m.add("Cylinder end plate",(.04,.135,.135),(x,0,0),2)
    for y in [-.053,.053]:
        for z in [-.053,.053]:m.add("Tie rod",(.39,.009,.009),(0,y,z),1,"cylinder")
motion=m.group("Motion")
m.add("Chrome piston rod",(.66,.025,.025),(.25,0,0),1,"cylinder",motion)
m.add("Tool coupling",(.07,.060,.060),(.57,0,0),2,"cylinder",motion)
m.add("Process pad",(.027,.15,.19),(.6055,0,0),3,parent=motion)
for x in [-.10,.10]:
    m.add("Pneumatic fitting",(.035,.020,.020),(x,.068,0),1,"cylinder",axis="y")
m.save()

m=Model("CellClamp")
m.add("Clamp base",(.24,.036,.56),(0,.016,0),2)
for z,sign in [(-.245,1),(.245,-1)]:
    m.add("Pneumatic jaw slide",(.15,.085,.09),(0,.055,z),0)
    parent=m.group("ClampJaw" if sign==1 else "ClampJawOpposite")
    m.add("Jaw support",(.09,.14,.029),(0,.095,z),1,parent=parent)
    m.add("Soft jaw",(.14,.095,.025),(0,.12,z-sign*.028),2,parent=parent)
m.pipe("Clamp supply",[(.1,.045,-.29),(.23,.025,-.29),(.24,-.08,-.40)],.006,14)
m.save()

m=Model("CellProduct")
motion=m.group("Motion")
m.add("Product housing",(.26,.145,.22),(0,.0725,0),6,parent=motion)
m.add("Product lid",(.245,.017,.207),(0,.1535,0),0,parent=motion)
m.add("Serial identification",(.13,.006,.07),(-.033,.1655,0),12,parent=motion)
for x in [-.105,.105]:
    for z in [-.085,.085]:m.add("Lid screw",(.006,.012,.012),(x,.1685,z),1,"cylinder",motion,axis="y")
mark=m.group("Processed")
m.nodes[motion]["children"].append(mark)
m.add("Inspection passed mark",(.045,.003,.045),(.075,.1665,0),7,parent=mark)
m.save()

m=Model("CellFloor")
m.add("Workshop floor",(200,.045,200),(0,-.0225,0),13)
m.save()