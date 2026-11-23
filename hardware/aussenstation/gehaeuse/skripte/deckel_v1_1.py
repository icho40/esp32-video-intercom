# Rueckdeckel v1.1: Platinenpfosten 10 mm höher (Z -26/50 -> -16/60), Rest wie gedruckte v1.0
# Eingang: stl/Rueckdeckel_v1.0.stl (Drucklage) + skripte/eingang/_teil_pfosten_v1.1.stl (SolidCraft, Baugruppenlage)
import struct,numpy as np,manifold3d as m3,os
D=os.path.dirname(os.path.abspath(__file__))
os.chdir(os.path.dirname(D))                                # gehaeuse/
exec(open(os.path.join(D,'merge.py')).read().split('out,ins=')[0])
def raw(f):
    d=open(f,'rb').read(); n=struct.unpack('<I',d[80:84])[0]
    return np.frombuffer(d[84:84+n*50],dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')]))['v'].reshape(-1,3).astype(np.float64)
t=raw("stl/Rueckdeckel_v1.0.stl")
t=np.stack([t[:,0],63-t[:,2],t[:,1]],1)                    # Drucklage -> Baugruppe (Umkehrung von save)
_,idx,inv=np.unique(np.round(t/1e-4).astype(np.int64),axis=0,return_index=True,return_inverse=True)
lid=m3.Manifold(m3.Mesh(vert_properties=t[idx].astype(np.float32),tri_verts=inv.reshape(-1,3).astype(np.uint32)))
print('v1.0',round(lid.volume(),1),lid.status())
def cyl(x,z,r,y0,y1): return m3.Manifold.cylinder(y1-y0,r,r,128).rotate([-90,0,0]).translate([x,y0,z])
for x in (70,120):
    for z in (-26,50):
        lid-=cyl(x,z,5.3,52.0,59.0)                          # alter Pfosten Ø10 weg (Platte innen bei Y 59, Loch endete davor bei Y 58,5)
lid+=load("skripte/eingang/_teil_pfosten_v1.1.stl")                         # neue Pfosten Z -16/60
save(lid,"stl/Rueckdeckel_v1.1.stl")
