/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

export default function App() {
  return (
    <div className="min-h-screen bg-[#151619] text-white p-8 font-mono">
      <div className="max-w-3xl mx-auto border border-[#8E9299] p-8 rounded-lg shadow-2xl">
        <h1 className="text-4xl font-bold mb-4 text-[#FF4444]">ARM Voxel Survival</h1>
        <p className="text-[#8E9299] mb-6 italic">
          Ruthlessly optimized C + Raylib voxel engine for low-RAM ARM devices.
        </p>
        
        <div className="space-y-4">
          <section className="border-l-4 border-[#FF4444] pl-4 py-2 bg-[#1a1b1e]">
            <h2 className="text-xl font-semibold mb-2">Project Status</h2>
            <p>This is a standalone C project. The source code is located in the root directory:</p>
            <ul className="list-disc list-inside mt-2 text-sm">
              <li><code className="text-[#FF4444]">main.c</code> - Core game engine</li>
              <li><code className="text-[#FF4444]">build.sh</code> - ARM build script</li>
              <li><code className="text-[#FF4444]">README.md</code> - Documentation</li>
            </ul>
          </section>

          <section className="py-4">
            <h2 className="text-xl font-semibold mb-2">Technical Specs</h2>
            <div className="grid grid-cols-2 gap-4 text-sm">
              <div className="p-3 border border-[#8E9299]/30 rounded">
                <span className="block text-[#8E9299]">Memory Limit</span>
                <span className="text-lg font-bold">350MB RAM</span>
              </div>
              <div className="p-3 border border-[#8E9299]/30 rounded">
                <span className="block text-[#8E9299]">Chunk Size</span>
                <span className="text-lg font-bold">16x16x128</span>
              </div>
              <div className="p-3 border border-[#8E9299]/30 rounded">
                <span className="block text-[#8E9299]">Optimization</span>
                <span className="text-lg font-bold">Greedy Meshing</span>
              </div>
              <div className="p-3 border border-[#8E9299]/30 rounded">
                <span className="block text-[#8E9299]">Target FPS</span>
                <span className="text-lg font-bold">30+ on ARMv7</span>
              </div>
            </div>
          </section>

          <div className="bg-[#FF4444] text-black p-4 rounded font-bold text-center">
            DOWNLOAD SOURCE TO COMPILE
          </div>
        </div>
      </div>
    </div>
  );
}

