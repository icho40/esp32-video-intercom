import sys,struct,numpy as np,manifold3d as m3
def load(f):
    d=open(f,'rb').read(); n=struct.unpack('<I',d[80:84])[0]
    t=np.frombuffer(d[84:84+n*50],dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')]))['v'].reshape(-1,3).astype(np.float64)
    q=np.round(t/1e-4).astype(np.int64)
    u,idx,inv=np.unique(q,axis=0,return_index=True,return_inverse=True)
    mesh=m3.Mesh(vert_properties=t[idx].astype(np.float32),tri_verts=inv.reshape(-1,3).astype(np.uint32))
    mf=m3.Manifold(mesh); 
    if mf.status()!=m3.Error.NoError: print('  NICHT manifold:',f,mf.status())
    return mf
def save(mf,f):
    mesh=mf.to_mesh(); v=np.array(mesh.vert_properties)[:,:3]; tr=np.array(mesh.tri_verts)
    v=np.stack([v[:,0],v[:,2],63-v[:,1]],1); v[:,2]-=v[:,2].min()   # Druckausrichtung: Aussenseite unten
    t=v[tr]; nrm=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]); nrm/=np.linalg.norm(nrm,axis=1,keepdims=True)+1e-12
    rec=np.zeros(len(t),dtype=np.dtype([('n','<3f4'),('v','<9f4'),('a','<u2')])); rec['n']=nrm; rec['v']=t.reshape(-1,9)
    open(f,'wb').write(b'\0'*80+struct.pack('<I',len(t))+rec.tobytes())
    print(f.split('/')[-1],'Volumen',round(mf.volume(),1),'Koerper',mf.decompose().__len__(),'Dreiecke',len(t),'Groesse',(v.max(0)-v.min(0)).round(2))
out,ins=sys.argv[1],sys.argv[2:]
r=load(ins[0])
for f in ins[1:]: r=r+load(f)
save(r,out)
