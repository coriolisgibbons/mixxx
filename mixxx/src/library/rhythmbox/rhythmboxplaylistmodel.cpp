#include <QtCore>
#include <QtGui>
#include <QtSql>

#include "library/trackcollection.h"
#include "library/rhythmbox/rhythmboxplaylistmodel.h"
#include "track/beatfactory.h"
#include "track/beats.h"

RhythmboxPlaylistModel::RhythmboxPlaylistModel(QObject* parent,
                                         TrackCollection* pTrackCollection)
        : BaseSqlTableModel(parent, pTrackCollection,
                            pTrackCollection->getDatabase(),
                            "mixxx.db.model.rhythmbox_playlist"),
          m_pTrackCollection(pTrackCollection),
          m_database(m_pTrackCollection->getDatabase()) {
    connect(this, SIGNAL(doSearch(const QString&)),
            this, SLOT(slotSearch(const QString&)));
}

RhythmboxPlaylistModel::~RhythmboxPlaylistModel() {
}

TrackPointer RhythmboxPlaylistModel::getTrack(const QModelIndex& index) const {
    QString artist = index.sibling(
        index.row(), fieldIndex("artist")).data().toString();
    QString title = index.sibling(
        index.row(), fieldIndex("title")).data().toString();
    QString album = index.sibling(
        index.row(), fieldIndex("album")).data().toString();
    QString year = index.sibling(
        index.row(), fieldIndex("year")).data().toString();
    QString genre = index.sibling(
        index.row(), fieldIndex("genre")).data().toString();
    float bpm = index.sibling(
        index.row(), fieldIndex("bpm")).data().toString().toFloat();
    QString location = index.sibling(
        index.row(), fieldIndex("location")).data().toString();

    TrackInfoObject* pTrack = new TrackInfoObject(location);
    pTrack->setArtist(artist);
    pTrack->setTitle(title);
    pTrack->setAlbum(album);
    pTrack->setYear(year);
    pTrack->setGenre(genre);
    pTrack->setBpm(bpm);
    TrackPointer track(pTrack, &QObject::deleteLater);

    // If the track has a BPM, then give it a static beatgrid.
    if (bpm > 0) {
        BeatsPointer pBeats = BeatFactory::makeBeatGrid(track, bpm, 0);
        track->setBeats(pBeats);
    }

    return track;
}

void RhythmboxPlaylistModel::search(const QString& searchText) {
    // qDebug() << "RhythmboxPlaylistModel::search()" << searchText
    //          << QThread::currentThread();
    emit(doSearch(searchText));
}

void RhythmboxPlaylistModel::slotSearch(const QString& searchText) {
    BaseSqlTableModel::search(searchText);
}

bool RhythmboxPlaylistModel::isColumnInternal(int column) {
    if (column == fieldIndex("track_id")) {
        return true;
    }
    return false;
}

Qt::ItemFlags RhythmboxPlaylistModel::flags(const QModelIndex &index) const {
    return readOnlyFlags(index);
}

void RhythmboxPlaylistModel::setPlaylist(QString playlist_path) {
    int playlistId = -1;
    QSqlQuery finder_query(m_database);
    finder_query.prepare(
        "SELECT id from rhythmbox_playlists where name='"+playlist_path+"'");
    if (!finder_query.exec()) {
        qDebug() << "SQL Error in RhythmboxPlaylistModel.cpp: line"
                 << __LINE__ << " " << finder_query.lastError();
        return;
    }

    while (finder_query.next()) {
        playlistId = finder_query.value(
            finder_query.record().indexOf("id")).toInt();
    }

    QString playlistID = "Rhythmboxplaylist_" + QString("%1").arg(playlistId);
    // Escape the playlist name
    QSqlDriver* driver = m_pTrackCollection->getDatabase().driver();
    QSqlField playlistNameField("name", QVariant::String);
    playlistNameField.setValue(playlistID);

    QStringList columns;
    columns << "track_id";
    columns << "position";

    QSqlQuery query(m_database);
    QString queryString = QString(
        "CREATE TEMPORARY VIEW IF NOT EXISTS %1 AS "
        "SELECT %2 FROM %3 WHERE playlist_id = %4")
            .arg(driver->formatValue(playlistNameField))
            .arg(columns.join(","))
            .arg("rhythmbox_playlist_tracks")
            .arg(playlistId);
    query.prepare(queryString);

    if (!query.exec()) {
        qDebug() << "Error creating temporary view for rhythmbox playlists. "
                 << "RhythmboxPlaylistModel --> line: "
                 << __LINE__ << " " << query.lastError();
        qDebug() << "Executed Query: " <<  query.executedQuery();
        return;
    }

    setTable(playlistID, columns[0], columns,
             m_pTrackCollection->getTrackSource("rhythmbox"));
    setDefaultSort(fieldIndex("position"), Qt::AscendingOrder);
    initHeaderData();
    setSearch("");
}

bool RhythmboxPlaylistModel::isColumnHiddenByDefault(int column) {
    return false;
}