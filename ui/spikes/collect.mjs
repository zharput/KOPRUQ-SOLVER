import http from 'node:http'; import fs from 'node:fs';
http.createServer((req, res) => { let b = ''; req.on('data', d => b += d); req.on('end', () => {
  res.writeHead(200, { 'Access-Control-Allow-Origin': '*', 'Access-Control-Allow-Headers': '*' }); res.end('ok');
  if (req.method === 'POST') { const n = req.url.slice(1); if (n.endsWith('_img')) fs.writeFileSync(n + '.png', Buffer.from(JSON.parse(b).png.split(',')[1], 'base64')); else fs.writeFileSync('result_' + n + '.json', b); } }); }).listen(5181, '127.0.0.1');
