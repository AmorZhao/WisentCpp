Frontend demo with C++ compiled wasm on Node.js runtime

```
...
├── client-app/
│   │
│   ├── app/ 
│   │   ├── api/ (routes...)
│   │   │
│   │   ├── layout.tsx
│   │   └── page.tsx
│   │
│   ├── styles/
│   │
│   └── public/wasm/
│       └── wisent.wasm
│
├── Misc/Wisent-standalone/
│   ├── WisentCpp.hpp
│   └── WisentServer.cpp
│
...
```

To compile Wisent into WebAssembly: 

```
./Misc/Wisent-standalone/compile-wasm.sh
```

To build / run Next.Js frontend locally: 

```
npm install
npm run dev
npm run build
npm run start
```

---

Reference: 
- [next.config.js](https://github.com/vercel/next.js/blob/canary/examples/with-webassembly/next.config.js)
- [Uncaught TypeError: WebAssembly.compile(): Argument 0 must be a buffer source](https://github.com/denoland/vscode_deno/issues/708)