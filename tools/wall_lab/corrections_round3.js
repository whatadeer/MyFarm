// Round-3 corrections, transcribed compactly: [id, w, h, floors, tiles[{xy,layer,role,correct(boulder)}]]
module.exports = {
corrections: [
 {id:"m5 N+S", w:5,h:5, floors:[[2,1],[2,3]], tiles:[
   {xy:[3,1],layer:"wall",role:"knw",correct:[6,16]},
   {xy:[1,1],layer:"wall",role:"kne",correct:[5,16]},
 ]},
 {id:"m7 N+E+S", w:5,h:5, floors:[[2,1],[3,2],[2,3]], tiles:[
   {xy:[1,1],layer:"wall",role:"kne",correct:[5,16]},
 ]},
 {id:"m13 N+S+W", w:5,h:5, floors:[[2,1],[2,3],[1,2]], tiles:[
   {xy:[3,1],layer:"wall",role:"knw",correct:[6,16]},
 ]},
 {id:"B NW+floorE", w:5,h:5, floors:[[1,1],[3,2]], tiles:[
   {xy:[2,1],layer:"wall",role:"knw",correct:[7,14]},
 ]},
 {id:"B NE+floorW", w:5,h:5, floors:[[3,1],[1,2]], tiles:[
   {xy:[2,1],layer:"wall",role:"kne",correct:[4,14]},
 ]},
 {id:"B SW+floorN", w:5,h:5, floors:[[1,3],[2,1]], tiles:[
   {xy:[2,2],layer:"wall",role:"w9",correct:[0,13]},
 ]},
 {id:"B SE+floorN", w:5,h:5, floors:[[3,3],[2,1]], tiles:[
   {xy:[2,2],layer:"wall",role:"w3",correct:[2,13]},
 ]},
 {id:"C N+S thin", w:5,h:6, floors:[[2,1],[2,4]], tiles:[
   {xy:[2,1],layer:"overlay",role:null,correct:[1,12]},
   {xy:[2,2],layer:"wall",role:"r1",correct:[1,14]},
   {xy:[3,2],layer:"wall",role:"fcr",correct:[6,13]},
   {xy:[1,2],layer:"wall",role:"fcl",correct:[5,13]},
 ]},
 {id:"C S+E wide notch", w:5,h:6, floors:[[3,2],[2,4]], tiles:[
   {xy:[3,3],layer:"wall",role:"w9",correct:[0,13]},
 ]},
 {id:"C S+W plain", w:5,h:6, floors:[[1,2],[2,4]], tiles:[
   {xy:[1,3],layer:"wall",role:"w3",correct:[2,13]},
 ]},
 {id:"C S+W jamb", w:5,h:6, floors:[[1,2],[2,4],[1,4]], tiles:[
   {xy:[0,2],layer:"wall",role:"kne",correct:[5,16]},
 ]},
 {id:"C N+E+S end", w:5,h:6, floors:[[2,1],[3,2],[2,4]], tiles:[
   {xy:[2,1],layer:"overlay",role:null,correct:[2,12]},
   {xy:[1,2],layer:"wall",role:"fcl",correct:[5,13]},
   {xy:[2,2],layer:"wall",role:"r3",correct:[2,14]},
   {xy:[3,3],layer:"wall",role:"w9",correct:[0,13]},
 ]},
 {id:"C N+S+W end", w:5,h:6, floors:[[2,1],[1,2],[2,4]], tiles:[
   {xy:[2,1],layer:"overlay",role:null,correct:[0,12]},
   {xy:[2,2],layer:"wall",role:"r9",correct:[0,14]},
   {xy:[3,2],layer:"wall",role:"fcr",correct:[6,13]},
   {xy:[1,3],layer:"wall",role:"w3",correct:[2,13]},
 ]},
 {id:"D ccne2", w:5,h:6, floors:[[3,4],[2,1]], tiles:[
   {xy:[2,1],layer:"overlay",role:null,correct:[1,12]},
   {xy:[2,2],layer:"wall",role:"ccne2",correct:[5,13]},
 ]},
 {id:"D ccnw2", w:5,h:6, floors:[[1,4],[2,1]], tiles:[
   {xy:[2,2],layer:"wall",role:"ccnw2",correct:[6,13]},
   {xy:[2,1],layer:"overlay",role:null,correct:[1,12]},
 ]},
 {id:"E junction fcl", w:6,h:6, floors:[[3,2],[3,4]], tiles:[
   {xy:[2,2],layer:"wall",role:"kne",correct:[5,16]},
   {xy:[4,2],layer:"wall",role:"knw",correct:[6,16]},
 ]},
 {id:"E junction fcr", w:6,h:6, floors:[[2,2],[2,4]], tiles:[
   {xy:[3,2],layer:"wall",role:"knw",correct:[6,16]},
   {xy:[1,2],layer:"wall",role:"kne",correct:[5,16]},
 ]},
 {id:"F v-thin middle", w:5,h:5, floors:[[1,1],[3,1],[1,2],[3,2],[1,3],[3,3]], tiles:[
   {xy:[4,3],layer:"wall",role:"w8",correct:[6,14]},
   {xy:[0,3],layer:"wall",role:"w2",correct:[5,14]},
   {xy:[2,3],layer:"wall",role:"w10",correct:[8,14]},
 ]},
 {id:"F v-thin top end", w:5,h:5, floors:[[1,1],[2,1],[3,1],[1,2],[3,2],[1,3],[3,3]], tiles:[
   {xy:[2,2],layer:"wall",role:"w11",correct:[3,13]},
   {xy:[2,3],layer:"wall",role:"w10",correct:[8,14]},
   {xy:[4,3],layer:"wall",role:"w8",correct:[6,14]},
   {xy:[0,3],layer:"wall",role:"w2",correct:[5,14]},
 ]},
 {id:"F v-thin bottom end", w:5,h:5, floors:[[1,2],[3,2],[1,3],[2,3],[3,3],[1,1],[3,1]], tiles:[
   {xy:[0,3],layer:"wall",role:"w2",correct:[5,14]},
   {xy:[4,3],layer:"wall",role:"w8",correct:[6,14]},
 ]},
 {id:"F L-turn ES", w:5,h:5, floors:[[0,0],[1,0],[2,0],[3,0],[4,0],[0,1],[1,1],[2,1],[0,2],[1,2],[0,3],[1,3],[0,4],[1,4],[3,4],[4,4],[3,3],[4,3],[4,2]], tiles:[
   {xy:[3,1],layer:"wall",role:"w15",correct:[7,15]},
   {xy:[2,2],layer:"wall",role:"w11",correct:[3,13]},
   {xy:[2,1],layer:"overlay",role:"w11",correct:[4,12]},
   {xy:[3,0],layer:"overlay",role:null,correct:[4,12]},
   {xy:[2,4],layer:"wall",role:"w10",correct:[8,14]},
 ]},
 {id:"F L-turn EW", w:5,h:5, floors:[[0,0],[1,0],[2,0],[3,0],[4,0],[2,1],[3,1],[4,1],[3,2],[4,2],[3,3],[4,3],[3,4],[4,4],[0,1],[0,2],[0,3],[0,4],[1,4]], tiles:[
   {xy:[1,1],layer:"wall",role:"w11",correct:[4,14]},
   {xy:[2,1],layer:"overlay",role:null,correct:[2,12]},
   {xy:[2,4],layer:"wall",role:"knw",correct:[8,14]},
 ]},
 {id:"F L-turn SW", w:5,h:5, floors:[[0,4],[1,4],[2,4],[3,4],[4,4],[0,3],[1,3],[2,3],[0,2],[1,2],[0,1],[1,1],[0,0],[1,0],[3,0],[4,0],[3,1],[4,1],[4,2]], tiles:[
   {xy:[2,1],layer:"wall",role:"w14",correct:[4,15]},
   {xy:[3,1],layer:"overlay",role:null,correct:[7,12]},
   {xy:[3,2],layer:"wall",role:"w15",correct:[4,15]},
 ]},
 {id:"F L-turn SE", w:5,h:5, floors:[[0,4],[1,4],[2,4],[3,4],[4,4],[2,3],[3,3],[4,3],[3,2],[4,2],[3,1],[4,1],[3,0],[4,0],[0,0],[1,0],[0,1],[1,1],[0,2],[0,3],[1,3]], tiles:[
   {xy:[2,1],layer:"wall",role:"w14",correct:[7,15]},
 ]},
 {id:"F T-junction", w:5,h:5, floors:[[3,0],[4,0],[3,1],[4,1],[3,2],[4,2],[3,3],[4,3],[3,4],[4,4],[0,2],[1,2]], tiles:[
   {xy:[2,2],layer:"wall",role:"knw",correct:[7,14]},
   {xy:[2,3],layer:"wall",role:"knw2",correct:[2,13]},
   {xy:[2,4],layer:"wall",role:"w2",correct:[5,14]},
 ]},
 {id:"F cross", w:5,h:5, floors:[[0,0],[1,0],[3,0],[4,0],[0,1],[1,1],[3,1],[4,1],[0,3],[1,3],[3,3],[4,3],[0,4],[1,4],[3,4],[4,4]], tiles:[
   {xy:[2,1],layer:"wall",role:"fcl",correct:[8,16]},
   {xy:[2,4],layer:"wall",role:"w10",correct:[8,14]},
 ]},
 {id:"F v-wall meets h-run", w:6,h:6, floors:[[1,0],[3,0],[1,1],[3,1],[1,2],[3,2],[0,4],[1,4],[3,4],[4,4],[5,4],[0,3],[5,3]], tiles:[
   {xy:[2,2],layer:"wall",role:"fcl",correct:[8,16]},
   {xy:[4,2],layer:"wall",role:"w14",correct:[7,15]},
 ]},
 {id:"G lone pillar", w:5,h:5, floors:[[2,1],[1,2],[3,2],[2,3],[1,1],[3,1],[1,3],[3,3]], tiles:[
   {xy:[4,3],layer:"wall",role:"w8",correct:[6,14]},
   {xy:[0,3],layer:"wall",role:"w2",correct:[5,14]},
 ]},
 {id:"G 2x1 horizontal", w:6,h:5, floors:[[1,1],[2,1],[3,1],[4,1],[1,2],[4,2],[1,3],[2,3],[3,3],[4,3]], tiles:[
   {xy:[5,3],layer:"wall",role:"w8",correct:[6,14]},
   {xy:[0,3],layer:"wall",role:"w2",correct:[5,14]},
 ]},
 {id:"G 1x3 vertical", w:5,h:6, floors:[[1,1],[3,1],[1,2],[3,2],[1,3],[3,3],[1,4],[3,4],[2,0],[2,5]], tiles:[
   {xy:[2,1],layer:"wall",role:"w11",correct:[3,13]},
   {xy:[4,4],layer:"wall",role:"w8",correct:[6,14]},
   {xy:[0,4],layer:"wall",role:"w2",correct:[5,14]},
 ]},
 {id:"G 2x2 block", w:6,h:6, floors:[[1,1],[2,1],[3,1],[1,2],[4,2],[1,3],[4,3],[1,4],[2,4],[3,4],[4,4],[4,1]], tiles:[
   {xy:[5,4],layer:"wall",role:"w8",correct:[6,14]},
   {xy:[0,4],layer:"wall",role:"w2",correct:[5,14]},
   {xy:[3,2],layer:"wall",role:"r3",correct:[2,14]},
   {xy:[2,2],layer:"wall",role:"r9",correct:[0,14]},
   {xy:[3,1],layer:"overlay",role:null,correct:[2,12]},
   {xy:[2,1],layer:"overlay",role:null,correct:[0,12]},
 ]},
],
// approved: the algorithm MUST keep producing these
approved: [
 {id:"m2 E", w:5,h:5, floors:[[3,2]], xy:[2,2], role:"w2"},
 {id:"m3 N+E", w:5,h:5, floors:[[2,1],[3,2]], xy:[2,2], role:"w3"},
 {id:"m4 S", w:5,h:5, floors:[[2,3]], xy:[2,2], role:"w4"},
 {id:"m6 E+S", w:5,h:5, floors:[[3,2],[2,3]], xy:[2,2], role:"w6"},
 {id:"m8 W", w:5,h:5, floors:[[1,2]], xy:[2,2], role:"w8"},
 {id:"m9 N+W", w:5,h:5, floors:[[2,1],[1,2]], xy:[2,2], role:"w9"},
 {id:"m10 E+W", w:5,h:5, floors:[[3,2],[1,2]], xy:[2,2], role:"w10"},
 {id:"m11 N+E+W", w:5,h:5, floors:[[2,1],[3,2],[1,2]], xy:[2,2], role:"w11"},
 {id:"m12 S+W", w:5,h:5, floors:[[2,3],[1,2]], xy:[2,2], role:"w12"},
 {id:"m14 E+S+W", w:5,h:5, floors:[[3,2],[2,3],[1,2]], xy:[2,2], role:"w15"},
 {id:"m15 all", w:5,h:5, floors:[[2,1],[3,2],[2,3],[1,2]], xy:[2,2], role:"w15"},
 {id:"B NW only", w:5,h:5, floors:[[1,1]], xy:[2,2], role:"knw"},
 {id:"B NE only", w:5,h:5, floors:[[3,1]], xy:[2,2], role:"kne"},
 {id:"B SW only", w:5,h:5, floors:[[1,3]], xy:[2,2], role:"w8"},
 {id:"B SE only", w:5,h:5, floors:[[3,3]], xy:[2,2], role:"w2"},
 {id:"B both top", w:5,h:5, floors:[[1,1],[3,1]], xy:[2,2], role:"kn2"},
 {id:"B both bottom", w:5,h:5, floors:[[1,3],[3,3]], xy:[2,2], role:"w10"},
 {id:"C S+E jamb", w:5,h:6, floors:[[3,2],[2,4],[3,4]], xy:[2,2], role:"r2"},
 {id:"D ccne", w:5,h:6, floors:[[3,4]], xy:[2,2], role:"ccne"},
 {id:"D ccnw", w:5,h:6, floors:[[1,4]], xy:[2,2], role:"ccnw"},
 {id:"E run left end", w:5,h:5, floors:[[0,1],[1,1],[2,1],[3,1],[4,1],[0,2],[4,2],[0,3],[1,3],[2,3],[3,3],[4,3]], xy:[1,2], role:"w13"},
 {id:"E run right end", w:5,h:5, floors:[[0,1],[1,1],[2,1],[3,1],[4,1],[0,2],[4,2],[0,3],[1,3],[2,3],[3,3],[4,3]], xy:[3,2], role:"w7"},
]
};
