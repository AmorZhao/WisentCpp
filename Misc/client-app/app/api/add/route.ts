import path from 'path';
import fs from 'fs';
import createWisentModule from '@/public/wasm/wisent.js'; 

export const runtime = 'nodejs';

function convertToNumber(given: string) {
  return parseInt(given, 10);
}

export async function GET(request: Request) 
{
  const url = new URL(request.url);
  const aParam = url.searchParams.get('a');
  const bParam = url.searchParams.get('b');

  if (!aParam || !bParam) {
    return new Response('Two inputs are required', { status: 400 });
  }

  const a = convertToNumber(aParam);
  const b = convertToNumber(bParam);

  const wasmPath = path.resolve(process.cwd(), 'public', 'wasm', 'wisent.wasm');
  const wasmBinary = fs.readFileSync(wasmPath);

  const instance = await createWisentModule({wasmBinary});
  const result = instance._simple_add(a, b);

  return new Response(`Result: ${result}`);
}
