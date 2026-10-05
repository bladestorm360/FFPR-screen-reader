#pragma once

// The extras area's public API: the gallery (FF1 Patches/GalleryPatches.cs, GalleryStateTracker)
// and the music player (FF1 Patches/MusicPlayerPatches.cs, MusicPlayerStateTracker).
//
// Other areas can ask by name instead: queries "GalleryStateTracker.IsInGallery" and
// "MusicPlayerStateTracker.IsInMusicPlayer" (the generic cursor reader stays silent while
// either is true). Main thread only.
namespace ffpr::extras {

/// GalleryStateTracker.IsInGallery.
bool is_in_gallery();
/// GalleryStateTracker.ClearState.
void clear_gallery_state();

/// MusicPlayerStateTracker.IsInMusicPlayer.
bool is_in_music_player();
/// MusicPlayerStateTracker.ClearState.
void clear_music_player_state();

} // namespace ffpr::extras
