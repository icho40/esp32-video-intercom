# Frontplatte: alte Namensschild-Ausschnitte füllen, LIRA NS1333f-Ausschnitte neu schneiden (manifold)
# Eingang skripte/eingang/Frontplatte_mit_Xiao_Pfosten.stl -> stl/Frontplatte_v1.0.stl (Baugruppenlage)
import struct,numpy as np,manifold3d as m3,os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
NS_B,NS_H,TASCHE_B,WAND,NS_X=71.3,19.3,79.5,1.5,92.0
ZS=(-10.0,20.0,50.0)
def load(p):
    b=open(p,'rb').read(); n=struct.unpack('<I',b[80:84])[0]
    rec=np.frombuffer(b[84:84+50*n],dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')]))
    v=rec['v'].reshape(-1,3).astype(np.float64)
    u,idx=np.unique(np.round(v,4),axis=0,return_inverse=True)
    return m3.Manifold(m3.Mesh(vert_properties=u.astype(np.float32),tri_verts=idx.reshape(-1,3).astype(np.uint32)))
def box(x0,x1,y0,y1,z0,z1): return m3.Manifold.cube([x1-x0,y1-y0,z1-z0]).translate([x0,y0,z0])
p=load('skripte/eingang/Frontplatte_mit_Xiao_Pfosten.stl')
print('Eingang',round(p.volume(),1),p.status())
for zc in ZS: p+=box(60.5,130.5,-6.0,-1.0,zc-12.5,zc+12.5)               # alte Tasche 68x23 + Durchbruch füllen
print('gefuellt',round(p.volume(),1))
for zc in ZS:
    p-=box(NS_X-NS_B/2,NS_X+NS_B/2,-7.0,0.0,zc-NS_H/2,zc+NS_H/2)          # Durchbruch 71,3 x 19,3
    p-=box(NS_X-TASCHE_B/2,NS_X+TASCHE_B/2,-6.0+WAND,0.0,zc-NS_H/2,zc+NS_H/2)  # Tasche hinten, Wand 1,5
mesh=p.to_mesh(); v=np.array(mesh.vert_properties)[:,:3]; t=v[np.array(mesh.tri_verts)]
nrm=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]); nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-12
rec=np.zeros(len(t),dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')])); rec['n']=nrm; rec['v']=t.reshape(-1,9)
open('stl/Frontplatte_v1.0.stl','wb').write(b'\0'*80+struct.pack('<I',len(t))+rec.tobytes())
print('Ausgang',round(p.volume(),1),v.min(0).round(2),v.max(0).round(2))
