comiket89-wlan-survey-book
==================

## なんだこれは
C89, 2015冬コミックマーケットで頒布した「Capturing 802.11 ひたすら収集するお話 2015」の文章データになります.

印刷に使った完成原稿(pdf)は含まれていません. また一部のリソースは意図的に劣化(白黒)ないし排除しています.
完成原稿はReVIEWのソースからコンパイルして生成してください.
なお印刷した完成原稿は OS X Yosemite 10.11.1, TexShop 3.58, review 1.72の環境でビルドしました.

## ビルドに必要なもの

- Ruby (1.9.3以上)
- [Re:VIEW](https://github.com/kmuto/review)
- TeX環境 (ebbやdvipdfmx等, またそこへのPATHが通っていること)

## ReVIEWへのパッチ

デフォルトのRe:VIEWで生成したpdfでは, 目次の文字色が赤くなってしまうため,
本書を生成するにあたっては以下の様なパッチを適用しました。

- Change TOC color from red to black (${REVIEWPATH}/lib/review/layout.tex.erb)
```diff
 \usepackage[dvipdfm,bookmarks=true,bookmarksnumbered=true,colorlinks=true,%
             pdftitle={<%= values["booktitle"] %>},%
             pdfauthor={<%= values["aut"] %>}]{hyperref}
+\hypersetup{ linkcolor=black }

 \newenvironment{reviewimage}{%
   \begin{figure}[H]
```

## ビルド方法
このソースのあるディレクトリでmakeコマンドを叩けば```c89book.pdf```ができあがるはず！

## 内容物
ReVIEWで必要となる画像データやsty以外の内容物について

(TBD)

## TeX 備忘録
毎季忘れるのでメモ

- http://osksn2.hep.sci.osaka-u.ac.jp/~taku/osx/install_ptex.html
- http://osksn2.hep.sci.osaka-u.ac.jp/~taku/osx/embed_hiragino.html
- http://tex.stackexchange.com/questions/88315/font-problem-errors-returned
