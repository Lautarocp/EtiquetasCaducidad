#pragma once
// html_content.h — Frontend completo embebido en PROGMEM
//
// El placeholder %PRODUCTS_JSON% se sustituye en tiempo de servicio
// por el array JSON de productos generado desde PRODUCTS[] en el .ino.

const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Etiquetas Caducidad</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#f0f4f8;padding:10px;max-width:480px;margin:0 auto}
h1{text-align:center;color:#1a237e;font-size:1.2em;padding:10px 0 12px}
.card{background:#fff;border-radius:8px;padding:13px;margin-bottom:10px;box-shadow:0 1px 3px rgba(0,0,0,.13)}
.card h2{font-size:.85em;color:#666;text-transform:uppercase;letter-spacing:.5px;margin-bottom:10px;border-bottom:1px solid #eee;padding-bottom:5px}
label{display:block;font-size:.78em;font-weight:700;color:#555;margin-bottom:2px;margin-top:7px}
label:first-of-type{margin-top:0}
input,select{width:100%;padding:9px 10px;border:1px solid #ccc;border-radius:5px;font-size:.95em;background:#fafafa;-webkit-appearance:none}
input:focus,select:focus{outline:none;border-color:#3949ab;background:#fff;box-shadow:0 0 0 2px rgba(57,73,171,.15)}
.row{display:flex;gap:8px}
.row>div{flex:1;min-width:0}
.btn{display:block;width:100%;padding:13px;border:none;border-radius:6px;font-size:1em;cursor:pointer;margin-bottom:8px;font-weight:700;transition:filter .12s}
.btn:active{filter:brightness(.88)}
.b-imp{background:#1565C0;color:#fff}
.b-test{background:#E65100;color:#fff}
#preview{font-family:'Courier New',monospace;font-size:.75em;white-space:pre;padding:10px 12px;background:#f9f9f9;border:1px solid #e0e0e0;border-radius:5px;min-height:88px;color:#333;line-height:1.55}
#st{padding:9px 12px;border-radius:5px;margin-bottom:10px;font-weight:700;font-size:.88em;display:none;text-align:center}
.ok{background:#e8f5e9;color:#1b5e20;border:1px solid #a5d6a7}
.er{background:#ffebee;color:#b71c1c;border:1px solid #ef9a9a}
.wt{background:#fff8e1;color:#e65100;border:1px solid #ffe082}
</style>
</head>
<body>
<h1>Etiquetas de Caducidad</h1>
<div id="st"></div>
<div class="card">
  <h2>Producto</h2>
  <label>Preset de producto</label>
  <select id="preset" onchange="onPreset()">
    <option value="">-- Seleccionar preset --</option>
  </select>
  <label>Nombre del producto *</label>
  <input type="text" id="producto" placeholder="Ej: Tortilla de patatas" oninput="upd()" autocomplete="off">
</div>
<div class="card">
  <h2>Fechas</h2>
  <div class="row">
    <div>
      <label>Realizaci&#xF3;n *</label>
      <input type="date" id="realizacion" onchange="onReal()">
    </div>
    <div>
      <label>Envasado *</label>
      <input type="date" id="envasado" onchange="upd()">
    </div>
  </div>
  <div class="row">
    <div>
      <label>Caducidad *</label>
      <input type="date" id="caducidad" onchange="upd()">
    </div>
    <div>
      <label>Congelado</label>
      <input type="date" id="congelado" onchange="upd()">
    </div>
  </div>
</div>
<div class="card">
  <h2>Opciones</h2>
  <label>N&#xFA;mero de copias</label>
  <input type="number" id="copias" value="1" min="1" max="10" style="width:90px">
</div>
<div class="card">
  <h2>Vista previa</h2>
  <div id="preview">Rellena el formulario...</div>
</div>
<button class="btn b-imp" onclick="send(false)">&#128438; Imprimir</button>
<button class="btn b-test" onclick="send(true)">&#9881; Imprimir prueba</button>
<script>
var P=%PRODUCTS_JSON%;
(function(){
  var s=document.getElementById('preset');
  P.forEach(function(p,i){
    var o=document.createElement('option');
    o.value=i; o.textContent=p.name+' ('+p.days+'d)'; s.appendChild(o);
  });
  var t=iso(new Date());
  document.getElementById('realizacion').value=t;
  document.getElementById('envasado').value=t;
  upd();
})();
function iso(d){return d.getFullYear()+'-'+pad(d.getMonth()+1)+'-'+pad(d.getDate())}
function pad(n){return n<10?'0'+n:n}
function fmt(v){if(!v)return'';var p=v.split('-');return p[2]+'/'+p[1]+'/'+p[0]}
function onPreset(){
  var i=document.getElementById('preset').value;
  if(i==='')return;
  document.getElementById('producto').value=P[+i].name;
  onReal();
}
function onReal(){
  var r=document.getElementById('realizacion').value;
  var i=document.getElementById('preset').value;
  if(r&&i!==''){
    var d=new Date(r+'T00:00:00');
    d.setDate(d.getDate()+P[+i].days);
    document.getElementById('caducidad').value=iso(d);
  }
  if(!document.getElementById('envasado').value)
    document.getElementById('envasado').value=document.getElementById('realizacion').value;
  upd();
}
function upd(){
  var nm=document.getElementById('producto').value||'(sin nombre)';
  var r=fmt(document.getElementById('realizacion').value)||'--/--/----';
  var e=fmt(document.getElementById('envasado').value)||'--/--/----';
  var c=fmt(document.getElementById('caducidad').value)||'--/--/----';
  var f=fmt(document.getElementById('congelado').value);
  var L=[
    '================================',
    ' '+nm.substring(0,22).toUpperCase(),
    '--------------------------------',
    'Realizacion: '+r,
    'Envasado:    '+e,
    '   CAD: '+c
  ];
  if(f)L.push('Congelado:   '+f);
  L.push('================================');
  document.getElementById('preview').textContent=L.join('\n');
}
function showSt(cls,msg){var s=document.getElementById('st');s.className=cls;s.textContent=msg;s.style.display='block'}
function ok(v){if(!v){alert('El nombre del producto es obligatorio.');return false}
  var r=document.getElementById('realizacion').value;
  var e=document.getElementById('envasado').value;
  var c=document.getElementById('caducidad').value;
  if(!r){alert('Fecha de realizaci\xF3n obligatoria.');return false}
  if(!e){alert('Fecha de envasado obligatoria.');return false}
  if(!c){alert('Fecha de caducidad obligatoria.');return false}
  if(c<r){alert('La caducidad debe ser posterior a la realizaci\xF3n.');return false}
  return true}
function send(test){
  var nm=document.getElementById('producto').value.trim();
  if(!test&&!ok(nm))return;
  var d={
    producto:nm,
    realizacion:fmt(document.getElementById('realizacion').value),
    envasado:fmt(document.getElementById('envasado').value),
    caducidad:fmt(document.getElementById('caducidad').value),
    congelado:fmt(document.getElementById('congelado').value),
    copias:parseInt(document.getElementById('copias').value)||1,
    test:test
  };
  showSt('wt','Enviando...');
  fetch('/print',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})
  .then(function(r){return r.json()})
  .then(function(r){
    if(r.ok)showSt('ok','✔ Impresi\xF3n enviada correctamente');
    else showSt('er','✗ Error: '+(r.msg||'respuesta desconocida'));
  })
  .catch(function(){showSt('er','✗ Sin conexi\xF3n con el ESP32')});
}
</script>
</body>
</html>
)rawliteral";
