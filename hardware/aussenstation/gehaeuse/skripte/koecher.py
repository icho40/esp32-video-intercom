# Baut den USB-Köcher sauber (manifold) aus den Maßen nach -> _teil_koecher.stl (Baugruppenlage)
import struct,numpy as np,manifold3d as m3,os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
XC,ZC=36.75,-30.385
def prism(cs,y0,y1): return m3.Manifold.extrude(cs,y1-y0).rotate([90,0,0]).translate([0,y1,0])
def stadium(halb,r): return (m3.CrossSection.square([2*halb,2*r]).translate([XC-halb,ZC-r])
        + m3.CrossSection.circle(r,128).translate([XC-halb,ZC]) + m3.CrossSection.circle(r,128).translate([XC+halb,ZC]))
def box(x0,x1,y0,y1,z0,z1): return m3.Manifold.cube([x1-x0,y1-y0,z1-z0]).translate([x0,y0,z0])
k=box(27.65,45.85,32.5,59.2,-36.5,-24.3)
k-=prism(stadium(3.0,3.65),31.0,62.2)                       # Buchsentasche 13,3 x 7,3
k-=m3.Manifold.batch_hull([prism(stadium(3.0,4.45),32.0,32.5),prism(stadium(3.0,3.65),33.29,33.3)])  # 0,8-Fase hinten
k-=box(28.9,44.6,34.5,38.5,-34.035,-22.0)                   # Gabelschlitz 4 mm, oben offen
mesh=k.to_mesh(); v=np.array(mesh.vert_properties)[:,:3]; t=v[np.array(mesh.tri_verts)]
nrm=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]); nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-12
rec=np.zeros(len(t),dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')])); rec['n']=nrm; rec['v']=t.reshape(-1,9)
open('skripte/eingang/_teil_koecher.stl','wb').write(b'\0'*80+struct.pack('<I',len(t))+rec.tobytes())
print('Koecher',round(k.volume(),1),v.min(0).round(2),v.max(0).round(2))
