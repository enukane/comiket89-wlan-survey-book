= イベント無線LANのキャプチャ

この章では, イベント会場で無線LANのキャプチャをすることのモチベーションや,
本書でとるアプローチについて述べます.

== モチベーション

イベント向けの無線LAN環境は過酷な環境です.
会場の環境, 来場者の規模にもよりますがそれなりに広いあるいは逆に狭い空間に大人数が集うため,
簡単に観測してみても酷そうなのが見て取れます.
たとえばコミケ会場であるビッグサイトの電波状況を WiSpy等で覗いてみると@<img>{01-westhall_mono}の様になります.

//image[01-westhall_mono][Chanalyzerのスクリーンショット: C85の時の西ホール]

なるほどたしかに自宅の環境と比べると, 無線LANアクセスポイントの数が表示しきれないほど
たくさん見えます. また WiSpy が表示している無線LANの周波数帯の使用率も真っ赤になっており,
かなり酷そうだと言えます.

ただし, AP の数が多ければ多いほど酷いのかというと単純にそうでない場合もあります.
その環境の@<em>{酷さ}を正確に把握しようとする場合, 同一チャネルに存在している
無線LAN クライアントがどのように振る舞っているかについても考慮する必要があります.

また, このような AP を一覧するツールで見える数はその時ごとのモノであり,
時々刻々と変化していくイベント会場において一時一時がどうであったかを追いかけるのには向いていません.
その空間がどのような状況なのか, そしてどのように変化していったのか, それをある程度正しく把握するためにはもう少し別のアプローチを取る必要があります.

#@#===[column] APの数が性能に与える影響
#@#APの数はそれなりに無線LANの速度に影響を与えます. 例えばAPが増えるごとにどれくらい理論上の伝送速度が落ちていくかを簡単にシミュレーションしてみると@<img>{01-apnum-vs-speed}のようになります. このシミュレーションは比較的牧歌的な想定で計算しているため@<fn>{apnum-vs-speed-script}, 実際にはここまで下がらない可能性はありますがそれでも数に応じて線形に帯域が狭くなるとは予想できます.
#@#
#@#//image[01-apnum-vs-speed][APの数 vs 802.11n の理論上の伝送速度][scale=0.4]
#@#//footnote[apnum-vs-speed-script][github.com/enukane/sim80211 の how-many-ap-disaster.rb 参照]

== 既存ソフト、製品の利用

前項にて WiSpy の図を提示しましたが, 似た様なことをできる既存ツールとして以下の様なものがあります.

: Wifi Analyzer (Androidアプリケーション)
  多くの方に使われているのはこのソフトウェアでしょう.
  前述の通りある程度の時間枠での, AP数の多さやチャネルごとの密集度合い, 個々のAPの電波強度はこれで簡単に把握することができます.
  デメリットとしては, 前述の通りAP数しか分からないこと, および時系列でのデータを保持できない点があります.
: WiSpy, その他スペクトラムアナライザ
  正しい意味で「電波状況」を観測する場合, スペクトラムアナライザを用いるのが一般的でしょう.
  どこの周波数にどの程度の電波がでているのかは正しくはこれでしか観測できません.
  デメリットとしてはまず値段が高いこと(十数万〜数百万円)が挙げられます.
  また, この中でも持ち運びに適した機材は一部しかありません.
  さらに実際の無線LANのアクティブティとの紐付けが単体ではできないことが挙げられます@<fn>{decode_ofdm}.
  スペアナはあくまである周波数で検知された電力/電圧を記録するものであり,
  これ単体ではその信号がそもそも無線LANのそれなのかどうかすらわかりません.
  WiSpy等では, ソフトウェア側でホストPCのスキャン情報と併せてプロットすることでこれを補っています.
  ただし, 記録している802.11の情報としては主にAPの電波強度とスペクトラムに限られています.
: Airmagnet Wi-Fi Analyzer
  モチベーションとしてやりたいことに一番近い既存ツールがこのAirmagnet社の製品になります.
  内部的にはひたすら無線LAN(802.11)のパケットをキャプチャし解析するといったことを行っているようです.
  情報の表示も潤沢であり, 内容に関しては文句はないのですが何より高いのが難点です.
  また、基本的にはWindowsマシンが必要であり携帯性を高めたい場合, タブレットPCの用意が必要です.

//footnote[decode_ofdm][もちろんOFDM/DSSS等の信号をデコードできるのであればこの限りではありません]

== 本書のアプローチ

本書では, APの数と電波強度以上の情報を外部観測および収集するために無線LANフレームのキャプチャを行います.
方式としては Airmagnet Wi-Fi Analyzer が取っているのと同じことをします.
これをコミケやその他イベントに持って行けるように以下の点を重視して作ってみます.

 * あつめる情報の多様性
 * キャプチャデバイスの携帯性, 経済性(システム全体含む)

本書ではこれを Raspberry Pi と USB Wi-Fi デバイスを用いて安価に実装することを試みています.
システム全体としては以下の様な動きを目指します.

//image[01-system_image][キャプチャデバイス, 解析系のシステムイメージ]

== これまでの取り組み

ここでは, 本書に至るまでに試みたプロトタイプ群の運用についてを一覧します.

//footnote[pcap80211analyzer][github.com/enukane/pcap80211analyzer]

 * コミケ85 (2013冬)
 ** http://www.slideshare.net/enukane/comiket-space-29723016
 *** Nexus7 (Android Pcap + USB Wi-Fi アダプタ)を用いて実施
 *** ソフトウェアとデバイスの制限で2.4GHz かつ 1チャネルのみ
 *** 解析は Wireshark のUI上にて手動で実施
 * コミケ86 (2014夏)
 ** http://www.slideshare.net/enukane/comiket-space-c86
 *** Raspberry Pi + USB Wi-Fi アダプタ + tshark を用いて実施
 *** チャネルは固定
 *** Raspberry Pi の制御は Nexus 7 + USB シリアルコンソールで実施
 * コミケ87 (2014冬)
 ** http://www.slideshare.net/enukane/c87-wifi-comiket-space
 *** Raspberry Pi にチャネル遷移(0.5秒毎)機能を導入
 *** 解析をスクリプト化@<fn>{pcap80211analyzer}
 * ニコニコ超会議2015 (2015/04)
 ** http://www.slideshare.net/enukane/chokaigi-2015-wifi-survey
 *** 構成はC87と同じ
 *** イベントで Free Wi-Fi を提供しているとのことで見物
 * 第十二回博麗例大祭 (2015/05)
 ** http://www.slideshare.net/enukane/reitaisai12-2015-wifi
 *** コミケとの比較のため見物 (ビッグサイト)

