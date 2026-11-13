#!/usr/bin/env node
// Read-only import from a desktop Termic checkout; never executes its store.
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
const root = process.argv[2];
if (!root) throw new Error('Usage: node scripts/amiga/import_prompts.mjs <desktop-checkout>');
const bodies = {};
for (const name of ['builtinPrompts.ts', 'review.ts']) {
  const source = fs.readFileSync(path.join(root, 'src/lib', name), 'utf8');
  Object.assign(bodies, await import('data:text/javascript,' + encodeURIComponent(source)));
}
const store = fs.readFileSync(path.join(root, 'src/store/prompts.ts'), 'utf8');
const block = store.match(/export const DEFAULT_PROMPTS[^=]*= \[([\s\S]*?)\n\];/)[1];
const note = 'Amiga execution context: use only the tools exposed by the selected agent. ' +
  'The native API agent can read project instructions and C/H files, edit C/H files and build; ' +
  'it has no shell, Git, web, PR or subagent tools. Do not pretend to perform unavailable steps. ' +
  'Ask for missing diffs or command output and report what remains unverified. ' +
  'Desktop shell examples below require a capable external agent and must be translated for AmigaDOS.\n\n';
const ascii = s => s.replace(/[\u2018\u2019]/g,"'").replace(/[\u201c\u201d]/g,'"')
  .replace(/[\u2013\u2014]/g,'-').replace(/\u2026/g,'...').replace(/\u2192/g,'->');
const prompts = [...block.matchAll(/id:\s*"([^"]+)",\s*title:\s*"([^"]+)",\s*body:\s*(\w+)/g)]
  .map(([,id,title,symbol]) => ({id,title,body:ascii(note+bodies[symbol]),enabled:true}));
if (prompts.length !== 17 || prompts.some(p => p.body.includes('undefined') || p.body.length>8192 || /[^\x09\x0a\x0d\x20-\x7e]/.test(p.body)))
  throw new Error('Unsupported default prompt shape or body');
fs.writeFileSync('amiga/native/Prompts/defaults.json', JSON.stringify(prompts,null,2)+'\n');
fs.writeFileSync('amiga/native/Prompts/provenance.json', JSON.stringify({
  repository:'https://github.com/SkiltonUSA/termic',
  commit:execFileSync('git',['-C',root,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),
  files:['src/store/prompts.ts','src/lib/builtinPrompts.ts','src/lib/review.ts'],
  license:'AGPL-3.0-or-later', adaptation:'Native tool-limit preamble and ASCII typography; upstream bodies retained.'
},null,2)+'\n');
console.log('Imported '+prompts.length+' Termic defaults.');
