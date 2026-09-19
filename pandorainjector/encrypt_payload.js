const fs = require('fs');
const path = require('path');

// Leer payload.h
const headerPath = path.join(__dirname, 'payload.h');
let content = fs.readFileSync(headerPath, 'utf8');

// Extraer el array de bytes
const match = content.match(/unsigned char payload\[\] = {([^}]+)}/);
if (match) {
    const bytesStr = match[1];
    const bytesArr = bytesStr.split(',').map(s => s.trim()).filter(s => s.length > 0);
    
    // Convertir a numeros, aplicar XOR y volver a formatear
    const xorKey = 0x93;
    const newBytes = bytesArr.map(b => {
        const val = parseInt(b, 16);
        const encrypted = val ^ xorKey;
        return '0x' + encrypted.toString(16).padStart(2, '0');
    });
    
    // Reconstruir
    let newContent = `const unsigned int payload_size = ${newBytes.length};\n`;
    newContent += `const unsigned char payload[] = {\n    ${newBytes.join(', ')}\n};\n`;
    
    fs.writeFileSync(headerPath, newContent, 'utf8');
    console.log(`[EXITO] Payload cifrado correctamente con XOR 0x93. Tamano: ${newBytes.length} bytes.`);
} else {
    console.log('[ERROR] No se pudo encontrar el array de payload. Asegurate de que el formato en payload.h sea correcto.');
}
