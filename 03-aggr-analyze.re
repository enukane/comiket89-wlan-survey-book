= キャプチャデータの集約と解析

本章では, 前章のデバイスで収集したキャプチャデータの処理について解説します.
これまでは, 採取した pcapng ファイルを手元PCにコピーして Wireshark で
開いて検分する, あるいは tshark で必要なフィールドのみ CSV 形式に切り出して
解析するといった方法をとっていました.
現在, これを embulk を用いて他のプラットフォームへ以降しその上で解析を行う
ように切り替えを進めています.

== キャプチャ後のワークフローとデータ解析

これまでにも述べた通り tochka は tshark を用いてフレームの収集をし,
pcapng 形式のバイナリファイルとしてローカルストレージ上にデータを溜め込むようになっています.
本書で目的とする無線LAN環境の解析にあたっては, このファイルからデータを取り出し
分析するといったワークフローを作る必要があります.

取り出しにあたっての課題は2つあり, ひとつは Raspberry Pi 上に貯まっているローカルのデータ
をどう外に引っ張りだすか, もうひとつは pcapng をどうパースするかです.

データのエクスポートという観点では, その使われ方の都合のため外に出られるネットワーク環境が
常にあるとは限らないという前提があります. またあったとしてもそれなりに
トラフィックが流れるためある程度の安定性が必要です.
このため基本的にはまずはキャプチャデバイスのローカルストレージに溜め,
あとで取り出すことにしています.

pcapng ファイルのパースについては, そのパースの面倒さが問題としてあります.
pcapng 形式自体は IETF でドラフトがでている程度にはオープンなバイナリフォーマット
ですが, 802.11 に限ったとしても共通ヘッダや Information Element のパース等を
手でやるのはしんどすぎます.
Wireshark/tshark には豊富にdissectorが導入されているため, フレームの細かいところまで表示してくれますが
その上で統計処理をするのには向いていません.


== tshark で中間表現に変換: pcap80211analyzer

pcap80211analyzer は昨年度にだした「Capturing 802.11」やこれまでslideshareに
挙げてきた各種スライド上の統計情報やグラフ生成をある程度自動化するために
作成したスクリプトです. 以下に例示されるような33個の情報について,
ターゲットのpcapngファイルから抽出したデータに基づいて計算を行います.

 * キャプチャの開始時刻, 終了時刻, 期間
 * 無線LAN AP, 無線LAN端末, 無線LANクライアントのユニーク数
 * チャネルごとの無線LAN AP, 無線LANクライアントの数
 * チャネルごとのタイプ別フレーム数のヒストグラム
 * 転送レートごとのフレーム数 (全帯域/2.4GHzのみ/5GHzのみ)
 * その他

Wireshark の CUI 版である tshark には@<list>{tshark_fields}のように
指定されたpcapngファイルの各フレームについて, 任意のフィールド
(ここでは wlan.ta と wlan.ra) を, 指定された区切り文字で出力する
という機能があります. この例ではCSV形式で出力がなされます.

//list[tshark_fields][tsharkを用いた特定フィールドの切り出し(wlan.ta, wlan.ra)]{
% tshark -r ${pcapngファイル} -T fields -E separator=, -e wlan.ta -e wlan.ra
//}

pcap80211analyzer は裏側でこの tshark コマンドの出力を CSV ファイルとして
生成およびキャッシュを行い, それに対して上記の解析を行っています.
フィールドとしては以下のみを扱っています.

 * frame.time_epoch (キャプチャ時刻)
 * frame.len (フレーム長, radiotapヘッダ含む)
 * radiotap.length (radiotapヘッダ)
 * radiotap.datarate (当該フレームの受信レート)
 * radiotap.flags.badfcs (FCSエラーの有無)
 * radiotap.dbm_antsignal (当該フレームの受信電波強度)
 * radiotap.channel.freq (キャプチャしたチャネルの周波数)
 * wlan.fc.type_subtype (フレームの802.11サブタイプ)
 * wlan.ta (802.11フレームの送信端末アドレス)
 * wlan.ra (802.11フレームの受信端末アドレス)
 * wlan.sa (802.11フレームの送信元アドレス)
 * wlan.bssid (802.11フレームのBSSID)


このスクリプト自体を Raspberry Pi で動かすのは非常に重いので,
実際の解析は以下のようなフローで手元のマシンやサーバにコピーしての実施をしていました.

 1. イベント会場にて, tochka (Raspbery Pi) でキャプチャを実施
 2. 持って帰り LAN ケーブルを接続
 3. SCP で解析実行サーバにファイル転送
 4. pcap80211analyzerにて解析

単体ないし少数のイベントおよびキャプチャファイルをターゲットとする場合はこれでも充分間に合います.
ただし複数日程の日ごとや複数イベントの比較等クロスで解析したい, 適切な形にデータを蓄積したい
といった柔軟性をもたせようとすると pcapng から CSV へのハードコーディングされた変換/移行部分を
どうにかしたほうがよさそうです.
また, 基本的にゴリゴリと自前の統計ロジックを書いている部分についても
既存のソフトや実装, 方法論に乗っかったほうがよいでしょう.


== embulkを用いたデータ移行

pcapng からのデータ抽出と解析部分を独立させるにあたり, embulk を導入しました.

embulk は fluentd を開発した Treasure Data 社がオープンソースで公開している
「fluentd のバッチ版のようなツール」です. fluentd はイベント・ログ・データを
逐次発生した時に流しこむことをターゲットとしていました.
これとは対照的に embulk では, 既にまとまった量として存在している DB やストレージ中の
ログやデータを一括で, または定期的に読み込むことに特化しています.

tochka ではイベント会期中に pcapng ファイルとしてまとめてフレームのデータをあつめて
持ち帰って回線のあるところで取り出すという形態を取っています.
この実際的にバルクロード部分に embulk を導入し, DBを通してキャプチャデータを扱えるようにします.


=== embulk-input-pcapng-files と embulk-parser-pcapng

pcapng を扱うための embulk プラグインとしては以下の2つを実装しています.

 1. embulk-input-pcapng-files
 2. embulk-parser-pcapng

前者は独立した input プラグイン, 後者は parser プラグインとして実装されています.
二者の大きな違いは対象となったファイル管理/アクセスの方法です.
前者では path カラムとしてターゲットのファイル名も併せて出力しますが, 後者ではこれを行いません.
このため複数 pcapng ファイルを読み込ませる必要があり, これらをファイル名にて区別したい場合,
前者では余計なフィールドが増える代わりに出自のパスまたはファイル名にて区別が付きます.
一方, 後者ではこれができません.
これは embulk-input-pcapng-files ではファイルの読み込み部分を自前で書いており
ファイル名をプラグイン側で保持している一方で,  embulk-parser-pcapng ではファイルの
読み込みロジックを file プラグインに依存しておりパスが渡されないためです.
時刻(frame.timeやframe.time_epoch)を抽出対象に入れれば, 例えばキャプチャ実行時間の違いから
区別をつけることは可能です. ただし, 同じ時間帯に行われた複数キャプチャを区別できないため,
このようなデータにおいて混同をさせたくない場合は前者を使っておくのが得策です.
本書ではこのような使い勝手の違いもあり embulk-input-pcapng-files をメインに用います.


==== embulk-input-pcapng-files

pcapng-files プラグインは, embulk コマンドにて以下の様にインストールが可能です.
なおこのプラグインの利用にあたっては事前に tshark コマンド(wireshark)のインストールが必要です.

//cmd{
% embulk gem install embulk-input-pcapng-files
//}

このプラグインを使うにあたっての embulk の設定例を@<list>{config_embulk_pcapng}に記載します.
この例では, カレントディレクトリ中の pcapng ファイルを探索し,
時刻 (frame.time_epoch), 当該ファイル中のフレーム番号 (frame.number),
802.11フレームの送受信者 (wlan.ta, wlan.ra), 802.11フレームの転送速度
(radiotap.datarate) を抽出しようとしています.

//list[config_embulk_pcapng][embulk-input-pcapng-filesのサンプルコンフィグ]{
in:
  type: pcapng_files
  paths: [ . ]
  convertdot: "-"
  use_basename: false
  schema:
    - { name: frame.time_epoch, type: timestamp }
    - { name: frame.number, type: long }
    - { name: wlan.ta, type: string }
    - { name: wlan.ra, type: string }
    - { name: radiotap.datarate, type: long }
//}

各フィールドの詳細は以下の通りです.

: type
  このプラグインのタイプ "pcapng_files" を指定
: paths
  pcapng ファイルを探索するディレクトリを指定. 再帰探索は実装していないため
  階層化されている場合は全ディレクトリを指定する必要があります.
  拡張子が "pcapng" であるファイルのみを読み込みます.
: convertdot
  pcapng のフィールド名に含まれるドット( . )を指定された文字列に変更. デフォルトは nil (変更なし).
  BigQuery などのアンダースコア以外の記号文字をカラム名として指定できない DB などに流し込みたい際に利用します.
: use_basename
  1カラム目に入る path (パス名)として拡張子を除いたファイル名を用いるかをtrue/falseで指定. デフォルト値は false. この場合, ターゲットの pcapng ファイルのフルパスが入ります.
  (例: path /home/test/pcap/thisispcap.pcapng を "thisispcap" に変更)
: schema
  pcapng ファイルから抽出したいフィールドのリスト. 配列としてそれぞれのハッシュの "name" にフィールド名,
  "type" に変換したい型を指定. 型は string, long, double, timestampのいずれかを指定可能です.
: done
  pcapng ファイル探索の除外リスト. embulk run 後にここに処理済みファイルが追記されます.

上記のコンフィグファイルを embulk コマンドの preview に指定して以下の様に実行すると,
@<img>{embulk_pcapng_es_preview}にあるように指定されたフィールドのみを抽出できているのが確認できます.

//cmd{
% embulk perview config.yml
//}

//image[embulk_pcapng_es_preview][embulk previewの実行結果][scale=0.5]

実際の流し込みにあたっては, 上記コンフィグファイルに output プラグイン側の記述も追記し,
embulk コマンドの run に渡してやる必要があります.

//cmd{
% embulk run config.yml
//}

次節以降では実際に Elasticsearch や本書の取り組みにてデータの蓄積先として用いている BigQuery
に流し込む方法について解説します.

=== embulk から elasticsearch, kibanaへの読み込み

ここでは簡単な可視化のために pcapng ファイルのデータを Elasticsearch + Kibana
の組み合わせに流し込んでみます.
筆者の環境では以下の様にセットアップを実施しました(OS X 10.11.1).

//cmd{
Elasticsearch インストール
% brew install home/versions/elasticsearch17
Elasticsearch用 elasticsearch-headプラグインのインストール
% plugin -i mobz/elasticsearch-head

Kibana のインストール
% brew install kibana

embulk用Elasticsearch向けoutputプラグインのインストール
% embulk gem install embulk-output-elasticsearch

elasticsearch の起動
% elasticsearch --cluster.name=embulk_pcapng_cluster

(別のターミナルで) kibanaの起動
% kibana
//}

embulkの設定ファイルとしては以下の様な記述をします.

//list[pcapng_embulk_es][pcapng to Elasticsearch: embulkの設定ファイル]{
in:
  type: pcapng_files
  paths: [ . ]
  convertdot: "-"
  threads: 1
  schema:
    - { name: frame.time_epoch, type: timestamp }
    - { name: frame.number, type: long }
    - { name: wlan.ta, type: string }
    - { name: wlan.ra, type: string }
    - { name: radiotap.datarate, type: long }
out:
  type: elasticsearch
  nodes:
    - {host: localhost}
  cluster_name: embulk_pcapng_cluster
  index: embulk_pcapng_load
  index_type: embulk_pcapng
//}

これに従って Elasticsearch 側にmappingを作成する必要があります.
ここでは pcapng2elasticsearch@<fn>{pcapng2elasticsearch}を用いてこれを作成し,
出力されたJSONファイルをcurlで投げ込んでいます.
このツールでは, string タイプはデフォルトで "index: not_analyzed" としています.
これは pcapng で特に解析の対象となりそうなアドレス部を下手に分割されないために仕込んでいます.
IPアドレスについては Elasticsearch のコアタイプに型が存在していますが,
特に無線LANで頻出のMACアドレス等はサポートされていないためこのような措置をとっています.


//footnote[pcapng2elasticsearch][https://github.com/enukane/embulk_pcapng-tools]

//list[es_mapping][pcapng to Elasticsearch: mappingの作成とデータの流し込み]{
mappingの生成
% pcapng2elasticsearch -i config.yml -o es_pcapng_mapping.json

以下の様な mapping が config.yml から作られる
% cat es_pcapng_mapping.json | jq .
{
  "mappings": {
    "embulk_pcapng": {
      "properties": {
        "path": {
          "type": "string"
        },
        "frame-time_epoch": {
          "type": "date"
        },
        "frame-number": {
          "type": "integer"
        },
        "wlan-ta": {
          "type": "string",
          "index": "not_analyzed"
        },
        "wlan-ra": {
          "type": "string",
          "index": "not_analyzed"
        },
        "radiotap-datarate": {
          "type": "integer"
        }
      }
    }
  }
}

Elasticsearchに設定
% curl -XPUT http://localhost:9200/embulk_pcapng_load -d "@pcapng_mapping.json"

pcapng から Elasticsearch へのロードの実行
% embulk run config.yml
//}

これらを設定した上でembulk runコマンドにより実際にデータをロードします.
筆者環境では約200万フレーム, 650MB程度の pcapng ファイルをロードするのに7分弱で終わりました.


これを Kibana で可視化・表示してみると @<img>{03-c88-d2-kibana} に記載したような
Dashboardを作ることができます. 今回はカラム数をかなり絞っていたため, 実際的に
有用なグラフは少ないですが, 時間当たりのフレーム数(FPS)や転送レートのヒストグラム,
送受信端末のユニーク数の計算・描画を簡単に行うことができます.

//image[03-c88-d2-kibana][Kibana での可視化例 (C88 2日目のキャプチャデータ)][scale=0.25]

=== BigQuery の利用

本節では, 実際に本書の取り組みで用いている BigQuery へのデータロードについて紹介します.
前節では Elasticsearch で解析, Kibana で結果の可視化を行いました.
このプラットフォームで解析を行うのは手軽ではあるのですが,
まずはひたすらいろんなイベントで集めた結果をどこかに集めるといったところに焦点が向いているため,
手放しでため込める場所が欲しいという要求があります.
このためにデータを保持するための自前サーバの構築や運用をするのが面倒といった理由により
お金を払って外部サービス, BigQuery に保存することにしました.
とはいえ今のところイベント会場無線LANのキャプチャデータは全体で100GB程度の容量(pcapng ファイルサイズ換算)
しかありません. そのまま全部突っ込んで同一容量分請求されるとしても, 
維持費だけなら月額2ドル程度とそこそこにお安くすみます.
テーブルをきちんと分ける等の対策をして, まかり間違って変なクエリをかけなければ
そこまでお財布に負担をかけることもきっとないだろうとの判断で利用しています@<fn>{bigquery_hasan}.

//footnote[bigquery_hasan][BigQueryで150万円溶かした人の顔 http://qiita.com/itkr/items/745d54c781badc148bb9]

tochka でキャプチャしたデータは, @<list>{pcapng_embulk_bq_out}に記載されている
設定ファイルのようにロードを行っています.
先ほどの Elasticsearch の時の例と打って変わってかなり多くのフィールドを対象としています.

//list[pcapng_embulk_bq_out][pcapng to bigquery の embulk 設定ファイル例]{
exec: {}
in:
  type: pcapng_files
  paths: [ /packets/Data/c88 ]
  threads: 2
  convertdot: "__"
  use_basename: true
  schema:
    - { name: frame.number,                 type: long }
    - { name: frame.time_epoch,             type: double }
    - { name: frame.len,                    type: long }
    - { name: radiotap.length,              type: long }
    - { name: radiotap.mactime,             type: long }
    - { name: radiotap.flags.preamble,      type: long }
    - { name: radiotap.flags.wep,           type: long }
    - { name: radiotap.flags.badfcs,        type: long }
    - { name: radiotap.flags.shortgi,       type: long }
    - { name: radiotap.datarate,            type: long }
    - { name: radiotap.channel.freq,        type: long }
    - { name: radiotap.channel.type.ofdm,   type: long }
    - { name: radiotap.channel.type.dynaic, type: long }
    - { name: radiotap.dbm_antsignal,       type: long }
    - { name: radiotap.dbm_antnoise,        type: long }
    - { name: radiotap.xchannel,            type: long }
    - { name: radiotap.xchannel.freq,       type: long }
    - { name: radiotap.xchannel.type.ofdm,  type: long }
    - { name: radiotap.mcs.gi,              type: long }
    - { name: radiotap.mcs.bw,              type: long }
    - { name: radiotap.mcs.index,           type: long }
    - { name: wlan.fc.type_subtype,         type: long }
    - { name: wlan.fc.retry,                type: long }
    - { name: wlan.ta,                      type: string }
    - { name: wlan.ra,                      type: string }
    - { name: wlan.sa,                      type: string }
    - { name: wlan.da,                      type: string }
    - { name: wlan.bssid,                   type: string }
    - { name: wlan_mgt.ssid,                type: string }
out:
  type: bigquery
  auth_method: private_key
  service_account_email: XXXXXXXX@developer.gserviceaccount.com
  p12_keyfile: /path/to/key.p12
  project: nk-bqtest
  dataset: pcapng
  table: c88
  formatter:
    type: csv
    header_line: false
  path_prefix: /path/to/cache/
  file_ext: csv.gz
//}

BigQueryに対しても事前に対しても事前にスキーマを設定を行います.
この際には pcapng2bigquery@<fn>{pcapng2bigquery}コマンドを用いています.
その他基本的な流れは bq コマンドを用いてテーブル作成をしていること以外は Elasticsearch の時と同様です.

//footnote[pcapng2bigquery][https://github.com/enukane/embulk_pcapng-tools]

//list[pcapng_embulk_pcapng2bq][スキーマの生成から本番投入まで]{
1. BigQuery用スキーマの生成
% pcapng2bigquery -i config-c88.yml -o c88.bq.json

2. BigQuery のテーブル作成
% bq mk -t nk-bqtest:pcapng.c88 ./c88.bq.json

3. プレビュー実行
% embulk preview config-c88.yml

4. 本番投入
% embulk run config-c88.yml
//}

一つポイント, 注意点としてはrun時には pcapng input プラグインの timestamp 型を使わない
方が良いという点があります. 具体的には, 上記の例では frame.time_epoch に対して timestamp 型を
指定してスキーマを生成(手順1)した後に, double 型に書き換えてバルクロードを行うことになります.

原因は調べ切れていないのですが, BigQuery に投入する際に時刻のフォーマットが規定の
"YYYY-MM-DD HH:MM:SS"@<fn>{timeformat_bq}ではなく "YYYY-MM-DD HH:MM:SS.ss"
といった表現になっているのが原因のように見受けられます.
BigQuery 側では DOUBLE でも時刻を投入できるためここでは無理くりごまかしてなんとかしています.

//footnote[timeformat_bq][https://cloud.google.com/bigquery/preparing-data-for-bigquery#datatypes]

== (補) キャプチャデバイスでのSORACOM SIM + fluentd を用いた流し込み

前節までの流れでは, embulk を既にキャプチャが完了したpcapng ファイルに対して行うという
流れに沿って解説をしてきました.
本節では, キャプチャデバイスから直接ストレージに対して流し込みを行う方法について
簡単に検証した際の結果を記載します.

そもそもなぜこの節で挙げるようなキャプチャデバイスからの直接の流し込みをしていなかったかと
言えば, キャプチャ環境の特性上ネットワークの準備が難しいから, というのが理由でした.
理想的には有線LANケーブルが引っ張ってこれるとベストですが,
もちろんお外でそんな環境は滅多にありません.
またテザリング等に頼ろうにも環境によっては人が多すぎて不安定といった問題がありました.
ところが最近はもっとも(?)混雑していると噂のコミケ会場でも
3G/LTEがそこそこ使える様になってきているのに加え, 格安SIMやSORACOM Airの登場で
こういった実験用の通信環境が簡単に手に入るようになりました(お金を少し払えば).


もちろんUSB 3Gデバイスの調達が必要であり, 第2章でも述べた通りUSBデバイスに対する
電源供給を考慮しなければなりません. その代わりにデバイス自身がインターネットに出られ,
その場でBigQuery ないし Elasticsearch 等のストレージ/DBにデータを投げ込めるという利点があります.
また副次的には, RTCで補っていた時刻源が不要になるという利点もあります.


この様な場合には embulk ではなく fluentd で中継して流し込むのが得策でしょう.
inputプラグインとして fluent-plugin-pcapng, outputプラグインとして
fluent-plugin-bigqueryを用いて以下の様なコンフィグファイルで実験的に運用をしてみました.

#@# ここにコンフィグファイル

//list[cofnig_fluent_pcapng][fluentdの設定ファイル: pcapng to BigQuery]{
<source>
  type pcapng
  id pcap_input

  tag pcap
  interface wlan0
  fields frame.time,frame.number,frame.time_epoch,frame.len,radiotap.length,radiotap.mac
time,radiotap.flags.preamble,radiotap.flags.wep,radiotap.flags.fcs,radiotap.flags.shortg
i,radiotap.datarate,radiotap.channel.freq,radiotap.channel.type.ofdm,radiotap.dbm_antsig
nal,radiotap.dbm_antnoise,radiotap.xchannel,radiotap.xchannel.freq,radiotap.xchannel.typ
e.ofdm,radiotap.mcs.gi,radiotap.mcs.bw,radiotap.mcs.index,wlan.fc.type_subtype,wlan.ta,w
lan.ra,wlan.sa,wlan.da
  types time,long,double,long,long,long,long,long,long,long,long,long,long,long,long,lon
g,long,long,long,long,long,long,string,string,string,string
  convertdot __
</source>

<match pcap>
  type bigquery
  method input

  auth_method private_key
  email XXXXXX@developer.gserviceaccount.com
  private_key_path /path/to/key.p12

  project nk-bqtest
  dataset soracom_test
  table   stream_test

  field_integer frame__number,frame__len,radiotap__length,radiotap__mactime,radiotap__fla
gs__preamble,radiotap__flags__wep,radiotap__flags__fcs,radiotap__flags__shortgi,radiotap_
_datarate,radiotap__channel__freq,radiotap__channel__type__ofdm,radiotap__dbm_antsignal,r
adiotap__dbm_antnoise,radiotap__xchannel,radiotap__xchannel__freq,radiotap__xchannel__typ
e__ofdm,radiotap__mcs__gi,radiotap__mcs__bw,radiotap__mcs__index,wlan__fc__type_subtype
  field_string frame__time,wlan__ta,wlan__ra,wlan__sa,wlan__da
  field_float frame__time_epoch
</match>
//}

fluent-plugin-pcapng は embulk-input-pcapng-files と同系統のプラグインとして作成しました.
設定ファイルの書式の違いはありますが, 設定できる内容はほとんど共通です.
embulk側のプラグインがファイル(pcapng)から読み込む一方, fluentd側のプラグインは
直接 tshark がインタフェースに張り付いてキャプチャを行います.
このため入力ソースとして pcapng のパスではなくインタフェース名を指定するという違いがあります.
なお, tochkaではキャプチャ時にチャネルを遷移させるロジックが組み込まれていますが,
このプラグインにはその機能はありません. このため, fluentd で回すのとは別にその裏側で
チャネル遷移をするデーモンを動かしておく必要があります.


8時間程度実験的にキャプチャ&ロードを行った結果,
収集できたフレームの総量は150万フレーム程度となっています.

どちらかと言えば気になるのはお財布事情ですが, BigQuery への Streaming Insert および
SORACOMに対してその実験日に支払った額はおおそ280円程度であり以下の様な内訳となっています.
日単位でしか計算されないため, 実験中のデータ分等が含まれている可能性があります.
目安程度にご参考ください.

 * BigQuery Streaming Insert: 8.42円
 ** 1465.574 MB (soracom経由以外の実験用のテスト分含む)
 * 通信費: 265円
 ** その日分として請求された実費
 ** Upload: s1.standard 1.0GiB
 ** Download: s1.standard 54.4MiB

この程度の量であればおよそ1台300円以下程度で日中8時間を運用することができます.

実際に不安なのは, 回線が実際に通信可能であるかどうかと太さが足りるかどうかです.
今回の場合, 1GBを8時間で転送しており300kbps程度の帯域があれば安定して送りつけられる計算です.
SORACOM Airの仕様上, s1.fastでは2Mbps, s1.standardでは512Kbpsと記載されていますが
これがあの環境でどこまで出るのか, どの程度安定するかという点がまず懸念点として上げられます.
この懸念については, C89で実際にコミケ会場で運用し検証を行う予定です.


またもう一つの懸念, というよりは実装上の都合ですが現状のfluent-plugin-pcapngには
pcapngファイルにも保存するという機能がありません. fluentdに流されるのは
設定ファイルで指定されたカラムのデータのみです.
その他のデータ(今回の場合ペイロードであったり他のInformation Elementの中身)は
tsharkの出力時から失われているため後から参照することができません.
