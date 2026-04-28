import { defineConfig } from 'vite'
import fs from 'node:fs'
import path from 'path'
import { execFileSync } from 'node:child_process'
import tailwindcss from '@tailwindcss/vite'
import react from '@vitejs/plugin-react'


function figmaAssetResolver() {
  return {
    name: 'figma-asset-resolver',
    resolveId(id) {
      if (id.startsWith('figma:asset/')) {
        const filename = id.replace('figma:asset/', '')
        return path.resolve(__dirname, 'src/assets', filename)
      }
    },
  }
}

function quizDataDevApi() {
  let repoRoot = path.resolve('C:/aa/build/external/quiz/quiz-data')

  const sendJson = (res, status, payload) => {
    res.statusCode = status
    res.setHeader('Content-Type', 'application/json; charset=utf-8')
    res.end(JSON.stringify(payload))
  }

  const readBody = (req) =>
    new Promise((resolve, reject) => {
      let body = ''
      req.on('data', chunk => { body += chunk })
      req.on('end', () => {
        try {
          resolve(body ? JSON.parse(body) : {})
        } catch (error) {
          reject(error)
        }
      })
      req.on('error', reject)
    })

  const normalizeRelativePath = (value = '') =>
    String(value)
      .replace(/\\/g, '/')
      .split('/')
      .map(part => part.trim())
      .filter(part => part && part !== '.' && part !== '..')
      .join('/')

  const repoPath = (relativePath) => {
    const safeRelativePath = normalizeRelativePath(relativePath)
    if (!safeRelativePath) throw new Error('Relative path is required.')

    const target = path.resolve(repoRoot, safeRelativePath)
    if (target !== repoRoot && !target.startsWith(`${repoRoot}${path.sep}`)) {
      throw new Error('Only quiz-data relative paths are allowed.')
    }
    return target
  }

  const git = (args) =>
    execFileSync('git', args, {
      cwd: repoRoot,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe'],
    }).trim()

  const gitOrEmpty = (args) => {
    try {
      return git(args)
    } catch {
      return ''
    }
  }

  const collectTree = (currentDir = repoRoot, entries = []) => {
    const children = fs.readdirSync(currentDir, { withFileTypes: true })
      .filter(entry => entry.name !== '.git')
      .sort((a, b) => a.name.localeCompare(b.name))

    for (const child of children) {
      const childPath = path.join(currentDir, child.name)
      const relativePath = path.relative(repoRoot, childPath).replace(/\\/g, '/')
      entries.push({ path: relativePath, kind: child.isDirectory() ? 'directory' : 'file' })
      if (child.isDirectory()) collectTree(childPath, entries)
    }

    return entries
  }

  const invoke = async (command, args = {}) => {
    switch (command) {
      case 'list_quiz_data_repositories':
        return [{
          name: path.basename(repoRoot),
          path: repoRoot,
          remote: fs.existsSync(path.join(repoRoot, '.git')) ? gitOrEmpty(['config', '--get', 'remote.origin.url']) : '',
          active: true,
        }]
      case 'select_quiz_data_repository': {
        const nextRoot = path.resolve(String(args.path || ''))
        if (!fs.existsSync(path.join(nextRoot, '.git'))) throw new Error('Git repository was not found.')
        repoRoot = nextRoot
        return `Git 레포 연결: ${repoRoot}`
      }
      case 'get_git_identity':
        return {
          name: gitOrEmpty(['config', 'user.name']),
          email: gitOrEmpty(['config', 'user.email']),
        }
      case 'set_git_identity':
        git(['config', 'user.name', String(args.name || '').trim()])
        git(['config', 'user.email', String(args.email || '').trim()])
        return 'Git 계정 저장 완료'
      case 'list_quiz_data_tree':
        return collectTree()
      case 'read_quiz_data_file':
        return fs.readFileSync(repoPath(args.relativePath), 'utf8')
      case 'write_quiz_data_file': {
        const target = repoPath(args.relativePath)
        fs.mkdirSync(path.dirname(target), { recursive: true })
        fs.writeFileSync(target, String(args.content ?? ''), 'utf8')
        return '저장 완료'
      }
      case 'create_quiz_data_file': {
        const target = repoPath(args.relativePath)
        if (fs.existsSync(target)) throw new Error('File already exists.')
        fs.mkdirSync(path.dirname(target), { recursive: true })
        fs.writeFileSync(target, '', 'utf8')
        return '파일 생성 완료'
      }
      case 'create_quiz_data_folder':
        fs.mkdirSync(repoPath(args.relativePath), { recursive: true })
        return '폴더 생성 완료'
      case 'delete_quiz_data_path': {
        const target = repoPath(args.relativePath)
        fs.rmSync(target, { recursive: true, force: true })
        return '삭제 완료'
      }
      case 'rename_quiz_data_path': {
        const source = repoPath(args.fromRelativePath)
        const target = repoPath(args.toRelativePath)
        fs.mkdirSync(path.dirname(target), { recursive: true })
        fs.renameSync(source, target)
        return '이름 바꾸기 완료'
      }
      case 'commit_quiz_data': {
        git(['add', '.'])
        const status = git(['status', '--porcelain'])
        if (!status) return '변경 사항이 없습니다.'
        git(['commit', '-m', String(args.message || '').trim() || 'Update quiz data'])
        return '커밋 완료'
      }
      case 'pull_quiz_data':
        git(['pull', '--rebase'])
        return 'Pull 완료'
      case 'push_quiz_data':
        git(['push'])
        return 'Push 완료'
      default:
        throw new Error(`Unsupported dev command: ${command}`)
    }
  }

  return {
    name: 'quiz-data-dev-api',
    configureServer(server) {
      server.middlewares.use('/__quiz-data/invoke', async (req, res) => {
        if (req.method !== 'POST') {
          sendJson(res, 405, { error: 'Method not allowed.' })
          return
        }

        try {
          const body = await readBody(req)
          const result = await invoke(body.command, body.args || {})
          sendJson(res, 200, { result })
        } catch (error) {
          sendJson(res, 500, { error: error instanceof Error ? error.message : String(error) })
        }
      })
    },
  }
}

export default defineConfig({
  base: './',
  build: {
    outDir: path.resolve(__dirname, '../../../build/out/quiz/quiz-editor/dist'),
    emptyOutDir: true,
  },
  plugins: [
    figmaAssetResolver(),
    quizDataDevApi(),
    // The React and Tailwind plugins are both required for Make, even if
    // Tailwind is not being actively used – do not remove them
    react(),
    tailwindcss(),
  ],
  resolve: {
    alias: {
      // Alias @ to the src directory
      '@': path.resolve(__dirname, './src'),
    },
  },

  // File types to support raw imports. Never add .css, .tsx, or .ts files to this.
  assetsInclude: ['**/*.svg', '**/*.csv'],
})
