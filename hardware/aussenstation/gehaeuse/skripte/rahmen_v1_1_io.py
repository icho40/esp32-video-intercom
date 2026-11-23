# Gemeinsame STL-Ein-/Ausgabe (manifold3d)
import struct,numpy as np,manifold3d as m3

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

