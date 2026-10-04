"""Generate FactoryCore's authored glTF fixtures. Uses only Python's standard library."""
import json, math, struct, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "Assets"
(ROOT / "Models").mkdir(parents=True, exist_ok=True)
(ROOT / "Textures").mkdir(parents=True, exist_ok=True)

def png(name, size, pixel):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + bytes(sum((list(pixel(x,y)) for x in range(size)), [])) for y in range(size))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB",size,size,8,2,0,0,0))
    data += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    (ROOT / "Textures" / name).write_bytes(data)

png("Grid.png", 128, lambda x,y: (48,62,75) if x<2 or y<2 else (108,122,134))
png("Belt.png", 128, lambda x,y: (38,130,100) if y%16<3 else (18,77,58))
png("Brushed.png",128,lambda x,y: (170+y%5*4,175+y%5*4,180+y%5*4))

def cube():
    faces=[
    ((1,0,0),[(.5,-.5,-.5),(.5,.5,-.5),(.5,.5,.5),(.5,-.5,.5)]),
    ((-1,0,0),[(-.5,-.5,.5),(-.5,.5,.5),(-.5,.5,-.5),(-.5,-.5,-.5)]),
    ((0,1,0),[(-.5,.5,-.5),(-.5,.5,.5),(.5,.5,.5),(.5,.5,-.5)]),
    ((0,-1,0),[(-.5,-.5,.5),(-.5,-.5,-.5),(.5,-.5,-.5),(.5,-.5,.5)]),
    ((0,0,1),[(-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5)]),
    ((0,0,-1),[(.5,-.5,-.5),(-.5,-.5,-.5),(-.5,.5,-.5),(.5,.5,-.5)])]
    p=[]; n=[]; u=[]; idx=[]
    for normal,points in faces:
        base=len(p)
        p+=points; n += [normal]*4; u += [(0,0),(1,0),(1,1),(0,1)]
        idx += [base,base+1,base+2,base,base+2,base+3]
    return p,n,u,idx

def cylinder():
    p=[]; n=[]; u=[]; idx=[]
    for j in range(32):
        a=j*math.tau/32; b=(j+1)*math.tau/32
        base=len(p)
        for x,t in [(-.5,a),(-.5,b),(.5,b),(.5,a)]:
            p.append((x,.5*math.cos(t),.5*math.sin(t)))
            n.append((0,math.cos(t),math.sin(t))); u.append((x+.5,t/math.tau))
        idx += [base,base+1,base+2,base,base+2,base+3]
        for sign, angles in [(1,(a,b)),(-1,(b,a))]:
            base=len(p)
            p.append((sign*.5,0,0)); n.append((sign,0,0)); u.append((.5,.5))
            for t in angles:
                p.append((sign*.5,.5*math.cos(t),.5*math.sin(t)))
                n.append((sign,0,0));u.append((.5+.5*math.cos(t),.5+.5*math.sin(t)))
            idx += [base,base+1,base+2]
    return p,n,u,idx

materials=[
{"name":"Anodized aluminium","pbrMetallicRoughness":{"baseColorFactor":[.68,.73,.79,1],"metallicFactor":.85,"roughnessFactor":.28,"baseColorTexture":{"index":0}}},
{"name":"Steel","pbrMetallicRoughness":{"baseColorFactor":[.75,.8,.85,1],"metallicFactor":.95,"roughnessFactor":.15}},
{"name":"Safety yellow","pbrMetallicRoughness":{"baseColorFactor":[.95,.53,.045,1],"metallicFactor":.15,"roughnessFactor":.35}},
{"name":"Sensor green","pbrMetallicRoughness":{"baseColorFactor":[.04,.75,.2,1],"metallicFactor":0,"roughnessFactor":.3},"emissiveFactor":[.01,.2,.04]},
{"name":"Blue polymer","pbrMetallicRoughness":{"baseColorFactor":[.025,.24,.68,1],"metallicFactor":.05,"roughnessFactor":.3}},
{"name":"Belt rubber","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":0,"roughnessFactor":.75,"baseColorTexture":{"index":1}}},
{"name":"Fault red","pbrMetallicRoughness":{"baseColorFactor":[.8,.02,.015,1],"metallicFactor":0,"roughnessFactor":.3},"emissiveFactor":[.8,.02,.015]},
{"name":"Floor grid","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"metallicFactor":.1,"roughnessFactor":.8,"baseColorTexture":{"index":2}}}]

def model(name,parts):
    data=bytearray(); views=[]; accessors=[]; meshes=[]; nodes=[]
    def accessor(values, kind, fmt, gltype, target):
        while len(data)%4:data.append(0)
        flat=[v for row in values for v in row] if kind!="SCALAR" else values
        start=len(data); data.extend(struct.pack("<"+fmt*len(flat),*flat))
        views.append({"buffer":0,"byteOffset":start,"byteLength":len(data)-start,"target":target})
        ac={"bufferView":len(views)-1,"componentType":gltype,"count":len(values),"type":kind}
        if kind=="VEC3":
            ac["min"]=[min(v[k] for v in values) for k in range(3)]
            ac["max"]=[max(v[k] for v in values) for k in range(3)]
        accessors.append(ac); return len(accessors)-1
    for part,shape,pos,scale,mat in parts:
        p,n,u,idx=(cube() if shape=="cube" else cylinder())
        if name=="Floor":u=[(a*30,b*30) for a,b in u]
        attrs={"POSITION":accessor(p,"VEC3","f",5126,34962),"NORMAL":accessor(n,"VEC3","f",5126,34962),"TEXCOORD_0":accessor(u,"VEC2","f",5126,34962)}
        indices=accessor(idx,"SCALAR","H",5123,34963)
        meshes.append({"name":part,"primitives":[{"attributes":attrs,"indices":indices,"material":mat}]})
        nodes.append({"name":part,"mesh":len(meshes)-1,"translation":pos,"scale":scale})
    doc={"asset":{"version":"2.0","generator":"FactoryCore asset generator"},"scene":0,"scenes":[{"nodes":list(range(len(nodes)))}],"nodes":nodes,"meshes":meshes,"materials":materials,
    "images":[{"uri":"../Textures/"+s} for s in ["Brushed.png","Belt.png","Grid.png"]],"textures":[{"source":i,"sampler":0} for i in range(3)],"samplers":[{"magFilter":9729,"minFilter":9987,"wrapS":10497,"wrapT":10497}],
    "buffers":[{"uri":name+".bin","byteLength":len(data)}],"bufferViews":views,"accessors":accessors}
    (ROOT/"Models"/(name+".bin")).write_bytes(data)
    (ROOT/"Models"/(name+".gltf")).write_text(json.dumps(doc,indent=2)+"\n",encoding="utf-8")

fault=("Fault","cube",[0,.7,0],[.06,.04,.06],6)
model("Cylinder",[("Body","cube",[0,.35,0],[.45,.15,.15],0),("Motion","cylinder",[.43,.35,0],[.45,.04,.04],1),("Mount","cube",[0,.12,0],[.25,.18,.2],4),fault])
model("Sensor",[("Body","cube",[0,.35,0],[.07,.1,.07],2),("Indicator","cube",[0,.405,0],[.045,.015,.045],3),fault])
model("Actuator",[("Body","cube",[0,.2,0],[.2,.15,.18],0),("Motion","cube",[0,.33,0],[.18,.05,.08],2),fault])
model("Motor",[("Body","cylinder",[0,.3,0],[.35,.2,.2],4),("Motion","cylinder",[.25,.3,0],[.3,.06,.06],1),fault])
model("Conveyor",[("Body","cube",[0,.2,0],[1.8,.15,.65],0),("Belt","cube",[0,.3,0],[1.7,.05,.56],5),("Motion","cube",[0,.335,0],[.035,.015,.55],2),
("LegLeft","cube",[-.65,.06,0],[.06,.2,.5],1),("LegRight","cube",[.65,.06,0],[.06,.2,.5],1),fault])
model("Workpiece",[("Motion","cube",[0,.43,0],[.13,.15,.18],4),fault])
model("Floor",[("Floor","cube",[0,-.03,0],[30,.05,30],7)])
print("Generated seven glTF models and three textures.")
