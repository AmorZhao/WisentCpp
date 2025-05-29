import path from 'path';
import fs from 'fs';
import createWisentModule from '@/public/wasm/wisent.js'; 

export const runtime = 'nodejs';

export async function POST(request: Request) 
{
  const wasmPath = path.resolve(process.cwd(), 'public', 'wasm', 'wisent.wasm');
  const wasmBinary = fs.readFileSync(wasmPath);
  const instance = await createWisentModule({
    wasmBinary,
    memory: new WebAssembly.Memory({ initial: 256, maximum: 65536, shared: true }),
  });

  await instance.ready;
  console.log(instance); 

  const url = new URL(request.url);
  const disableRLE = url.searchParams.get('disableRLE') === 'true' ? 1 : 0;
  const disableCsvHandling = url.searchParams.get('disableCsvHandling') === 'true' ? 1 : 0;
  const csvPrefix = url.searchParams.get('csvPrefix');

  const formData = await request.formData();
  const inputDataEntry = formData.get('inputData');
  if (!inputDataEntry) {
    throw new Error("Invalid or missing 'inputData' in form data");
  }
  let inputData: Uint8Array;
  if (typeof inputDataEntry === 'string') {
    inputData = new TextEncoder().encode(inputDataEntry);
  } 
  else if (inputDataEntry instanceof File) {
    const buffer = await inputDataEntry.arrayBuffer();
    inputData = new Uint8Array(buffer);
  } 
  else {
    throw new Error("Invalid 'inputData' type in form data");
  }
  const inputSize = inputData.length;
  const inputPtr = instance._malloc(inputSize);
  instance.HEAPU8.set(inputData, inputPtr);

  const strLen = instance.lengthBytesUTF8(csvPrefix) + 1;
  const strPtr = instance._malloc(strLen);
  instance.stringToUTF8(csvPrefix, strPtr, strLen);

  console.log(inputPtr, inputSize, strPtr);
  console.log(instance.HEAPU8.slice(inputPtr, inputPtr + inputSize));
  console.log(instance.HEAPU8.slice(strPtr, strPtr + strLen));

  if(formData.has('csvPreload') === true) {
    const csvFilename = formData.get('csvFilename');
    
    const file = formData.get('csvPreload') as File;
    const buffer = await file.arrayBuffer();
    
    const byteArray = new Uint8Array(buffer);
    // instance.FS_createPath("", csvPrefix, true, true);

    if (!instance.FS.analyzePath("/file").exists) {
      instance.FS.mkdir("/file");
    }
    instance.FS_createDataFile("", csvPrefix + "/" + csvFilename, byteArray, true, true);

      const filePath = `${csvPrefix}/${csvFilename}`;
    if (instance.FS_analyzePath(filePath).exists) {
      console.log(`File successfully created at: ${filePath}`);
      // const fileContent = instance.FS_readFile(filePath);
      // console.log(`File content: ${fileContent}`);
    } else {
      console.error(`Failed to create file at: ${filePath}`);
    }
  }

  try {
    const rootExpression = instance._loadWisent(
      inputPtr,
      inputSize, 
      strPtr,
      strLen,
      disableRLE, 
      disableCsvHandling
    );
    console.log("Root Expression:", rootExpression);
    return new Response(`Serialised Wisent Root: ${rootExpression}`);
  } catch (error) {
    console.error("Error:", error);
    return new Response(`Error: ${error}`, { status: 500 });
  } 
  finally {
    instance._free(inputPtr);
    instance._free(strPtr);
  }
}

