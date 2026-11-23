# Rahmen_v1.1: Antennenloch (SMA-Durchfuehrung) als D-Loch mit Verdrehschutz
# Basis stl/Rahmen_v1.0.stl: Loch Ø6,5 in der 2-mm-Haut X146..148, Mitte Y20/Z100, innen Ø14-Senkung
# Buchse gemessen: Gewinde Ø6,2, abgeflacht auf 5,7 -> Flaeche oben 2,75 ueber Mitte (6,0 ueber Flaeche, 0,3 Spiel)
import struct,numpy as np,manifold3d as m3,os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
YC,ZC,FLACH=20.0,100.0,2.75

def load(p):
    b=open(p,'rb').read(); n=struct.unpack('<I',b[80:84])[0]
    t=np.frombuffer(b[84:84+n*50],dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')]))['v'].reshape(-1,3).astype(np.float64)
    v,idx=np.unique(t.round(5),axis=0,return_inverse=True)
    return m3.Manifold(m3.Mesh(vert_properties=v.astype(np.float32),tri_verts=idx.reshape(-1,3).astype(np.uint32)))

def save(k,p):
    mesh=k.to_mesh(); v=np.array(mesh.vert_properties)[:,:3]; t=v[np.array(mesh.tri_verts)]
    nrm=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]); nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-12
    rec=np.zeros(len(t),dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')])); rec['n']=nrm; rec['v']=t.reshape(-1,9)
    open(p,'wb').write(b'\0'*80+struct.pack('<I',len(t))+rec.tobytes())

r=load('stl/Rahmen_v1.0.stl')
assert r.status()==m3.Error.NoError, r.status()
# Kreissegment oberhalb der Flaeche, etwas groesser als das Loch, damit es mit der Wand verschmilzt.
# 2D-Skizze mit x = -Z, y = Y; Drehung um Y um +90 Grad macht (x,y,z) -> (z,y,-x): Extrusion wird +X.
seg=(m3.CrossSection.circle(3.6,128).translate([-ZC,YC])
     ^ m3.CrossSection.square([10,10]).translate([-ZC-FLACH-10,YC-5]))
teil=m3.Manifold.extrude(seg,2.0).rotate([0,90,0]).translate([146.0,0,0])
print('Segment', np.round(teil.bounding_box(),3))
neu=r+teil
print('Volumen v1.0 %.1f  Segment %.2f  v1.1 %.1f  Zuwachs %.2f'%(r.volume(),teil.volume(),neu.volume(),neu.volume()-r.volume()))
save(neu,'stl/Rahmen_v1.1.stl')
