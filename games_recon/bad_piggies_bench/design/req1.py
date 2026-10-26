import pickle, sys, csv, collections
S = r"C:\Users\hamed\AppData\Local\Temp\claude\D--repo-Win32Craft-main\7ed8744f-0d72-482c-82d8-cd88a334a2ff\scratchpad"
sys.path.insert(0, r"D:\repo\Win32Craft-main\games_recon\bad_piggies_shader_proto\tools")
d = pickle.load(open(S + r"\out\shaders.pkl", "rb"))
print(type(d), len(d) if hasattr(d, '__len__') else '')
if isinstance(d, dict):
    print(list(d.keys())[:10])
    first = d[list(d.keys())[0]]
else:
    first = d[0]
print(type(first))
if isinstance(first, dict):
    print(list(first.keys()))
    for k, v in first.items():
        if k not in ('blob',):
            s = repr(v)
            print(k, s[:300])
