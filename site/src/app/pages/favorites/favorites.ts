import { Component, OnInit, inject, signal, effect } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterLink } from '@angular/router';
import { ApiService, WorkshopItem } from '../../core/services/api.service.js';
import { AuthService } from '../../core/services/auth.service.js';

@Component({
  selector: 'app-favorites',
  standalone: true,
  imports: [CommonModule, RouterLink],
  template: `
    <div class="workshop-container">
      <div class="section-header">
        <h2>
          <img src="/icons/star.png" class="silk-icon" alt="Favorites" />
          My Favorited Items ({{ items().length }})
        </h2>
      </div>

      <br />

      @if (loading()) {
        <div class="loading-state">
          <img src="/icons/arrow_refresh.png" class="silk-icon spinner" alt="Loading" />
          <p>Loading your favorites...</p>
        </div>
      } @else if (items().length === 0) {
        <div class="empty-state card">
          <div class="card-body">
            <img src="/icons/star.png" class="silk-icon-lg" alt="Star" />
            <h3>No favorites yet</h3>
            <p class="text-muted">Click the ★ star button on any item in the Workshop to bookmark it here!</p>
            <a routerLink="/workshop" class="btn">Explore Workshop</a>
          </div>
        </div>
      } @else {
        <div class="items-grid">
          @for (item of items(); track item.id) {
            <div class="card item-card">
              <div class="card-header">
                <div class="item-title-meta">
                  <span
                    class="badge"
                    [class.badge-blue]="item.type === 'set'"
                    [class.badge-green]="item.type === 'save'"
                    [class.badge-yellow]="item.type === 'stamp'"
                    [class.badge-purple]="item.type === 'theme'"
                  >
                    @if (item.type === 'set') {
                      <img src="/icons/package.png" class="silk-icon" alt="Set" /> Set
                    } @else if (item.type === 'save') {
                      <img src="/icons/disk.png" class="silk-icon" alt="Save" /> Save
                    } @else if (item.type === 'stamp') {
                      <img src="/icons/shape_handles.png" class="silk-icon" alt="Stamp" /> Stamp
                    } @else {
                      <img src="/icons/paintcan.png" class="silk-icon" alt="Theme" /> Theme
                    }
                  </span>
                  @if (item.version && item.version > 1) {
                    <span class="badge badge-version">v{{ item.version }}</span>
                  }
                  <a
                    [routerLink]="item.type === 'set' ? ['/workshop/sets', item.id] : ['/workshop/items', item.id]"
                    class="item-title-link"
                    [title]="item.title"
                  >
                    {{ item.title }}
                  </a>
                </div>
                <span class="author-tag">by <strong>{{ item.author }}</strong></span>
              </div>

              @if (item.thumbnail_path) {
                <div class="card-thumb-container">
                  <img [src]="api.getThumbnailUrl(item.id)" alt="{{ item.title }}" class="item-card-thumb" loading="lazy" />
                </div>
              }

              <div class="card-body item-body">
                @if (item.parent_set_title) {
                  <div class="parent-set-pill">
                    <img src="/icons/package.png" class="silk-icon" alt="Set" />
                    <span>Uses Set: </span>
                    <a [routerLink]="['/workshop/sets', item.parent_set_id]" class="set-link">
                      {{ item.parent_set_title }}
                    </a>
                  </div>
                }

                <p class="item-desc">{{ item.description || 'No description provided.' }}</p>

                <div class="meta-tags">
                  @if (parseMeta(item.meta_json); as m) {
                    @if (m.materials_count) {
                      <span class="meta-pill"><img src="/icons/palette.png" class="silk-icon" alt="mat" /> {{ m.materials_count }} materials</span>
                    }
                    @if (m.rules_count) {
                      <span class="meta-pill"><img src="/icons/script_code.png" class="silk-icon" alt="rules" /> {{ m.rules_count }} rules</span>
                    }
                    @if (getDimensions(item); as dims) {
                      <span class="meta-pill highlight"><img src="/icons/shape_square.png" class="silk-icon" alt="dim" /> {{ dims }}</span>
                    }
                    @if (item.child_saves_count && item.child_saves_count > 0) {
                      <span class="meta-pill highlight"><img src="/icons/disk.png" class="silk-icon" alt="saves" /> {{ item.child_saves_count }} community save{{ item.child_saves_count === 1 ? '' : 's' }}</span>
                    }
                  }
                </div>
              </div>

              <div class="card-footer item-actions">
                <button (click)="openInSand3(item, $event)" class="btn btn-sm btn-primary" title="Open in Sand3">
                  <img src="/icons/application_go.png" class="silk-icon" alt="Open" />
                  Open
                </button>

                <button class="btn btn-sm" [class.active]="item.is_liked" (click)="toggleLike(item, $event)" title="Like item">
                  <img src="/icons/heart.png" class="silk-icon" alt="Like" />
                  <span>{{ item.likes_count }}</span>
                </button>

                <button class="btn btn-sm active" (click)="removeFavorite(item, $event)" title="Remove from favorites">
                  <img src="/icons/star.png" class="silk-icon" alt="Favorite" />
                  <span>{{ item.favorites_count }}</span>
                </button>
              </div>
            </div>
          }
        </div>
      }

      @if (showUsageModal()) {
        @if (currentOpenItem(); as it) {
          <div class="modal-backdrop" (click)="closeUsageModal()">
            <div class="modal-card" (click)="$event.stopPropagation()">
              <div class="modal-header">
                <h2>
                  <img src="/icons/help.png" class="silk-icon" alt="Help" />
                  How to use in Sand3
                </h2>
                <button class="close-btn" (click)="closeUsageModal()">&times;</button>
              </div>

              <div class="modal-body">
                <p class="subtext">
                  You can load this {{ it.type }} directly inside Sand3:
                </p>

                <ol style="padding-left: 20px; line-height: 1.8; margin-bottom: 20px; color: var(--text);">
                  <li>Open <strong>Sand3</strong>.</li>
                  <li>Go to the <strong>Workshop</strong> tab to browse and activate with 1-click.</li>
                  <li>Alternatively, your file has been downloaded. Drag it into the Sand3 saves folder inside the correct set.</li>
                </ol>

                <div
                  style="background: rgba(255, 255, 255, 0.03); padding: 12px 14px; border-radius: 6px; border: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; gap: 10px;">
                  <span style="font-size: 13px; color: var(--text-muted);">Need the file again?</span>
                  <a [href]="getDownloadUrl(it.id)" class="btn btn-sm btn-primary" download>
                    <img src="/icons/arrow_down.png" class="silk-icon" alt="Download" /> Download file
                  </a>
                </div>
              </div>

              <div class="modal-footer">
                <button class="btn btn-sm" (click)="closeUsageModal()">Got it</button>
              </div>
            </div>
          </div>
        }
      }
    </div>
  `,
  styleUrl: '../workshop/workshop.scss'
})
export class FavoritesComponent implements OnInit {
  api = inject(ApiService);
  auth = inject(AuthService);
  items = signal<WorkshopItem[]>([]);
  loading = signal<boolean>(true);
  showUsageModal = signal<boolean>(false);
  currentOpenItem = signal<WorkshopItem | null>(null);

  constructor() {
    effect(() => {
      const _user = this.auth.currentUser();
      this.loadFavorites();
    });
  }

  ngOnInit() {
    this.loadFavorites();
  }

  loadFavorites() {
    if (!this.auth.currentUser()) {
      this.items.set([]);
      this.loading.set(false);
      return;
    }
    this.loading.set(true);
    this.api.getUserFavorites().subscribe({
      next: (favs) => {
        this.items.set(favs);
        this.loading.set(false);
      },
      error: () => this.loading.set(false)
    });
  }

  toggleLike(item: WorkshopItem, event: Event) {
    event.stopPropagation();
    if (!this.auth.currentUser()) {
      alert('Please log in to like workshop items.');
      return;
    }
    const prevLiked = !!item.is_liked;
    const prevCount = item.likes_count || 0;
    const optimisticLiked = !prevLiked;
    const optimisticCount = Math.max(0, prevCount + (optimisticLiked ? 1 : -1));


    this.items.update((list) =>
      list.map((i) => (i.id === item.id ? { ...i, is_liked: optimisticLiked, likes_count: optimisticCount } : i))
    );

    this.api.toggleLike(item.id).subscribe({
      next: (res) => {
        this.items.update((list) =>
          list.map((i) => (i.id === item.id ? { ...i, is_liked: res.is_liked, likes_count: res.likes_count } : i))
        );
      },
      error: () => {

        this.items.update((list) =>
          list.map((i) => (i.id === item.id ? { ...i, is_liked: prevLiked, likes_count: prevCount } : i))
        );
      }
    });
  }

  removeFavorite(item: WorkshopItem, event?: Event) {
    if (event) {
      event.stopPropagation();
    }
    const previousList = this.items();


    this.items.update((list) => list.filter((i) => i.id !== item.id));

    this.api.toggleFavorite(item.id).subscribe({
      next: () => {

      },
      error: () => {

        this.items.set(previousList);
      }
    });
  }

  openInSand3(it: WorkshopItem, event?: Event) {
    if (event) {
      event.stopPropagation();
    }
    this.currentOpenItem.set(it);
    const uri = `sand3://open?type=${it.type}&id=${it.id}`;
    let appOpened = false;

    const onBlur = () => {
      appOpened = true;
    };
    window.addEventListener('blur', onBlur, { once: true });

    window.location.href = uri;

    setTimeout(() => {
      window.removeEventListener('blur', onBlur);
      if (!appOpened) {
        const dlLink = document.createElement('a');
        dlLink.href = this.getDownloadUrl(it.id);
        dlLink.download = '';
        document.body.appendChild(dlLink);
        dlLink.click();
        document.body.removeChild(dlLink);

        this.showUsageModal.set(true);
      }
    }, 1200);
  }

  closeUsageModal() {
    this.showUsageModal.set(false);
  }

  getDownloadUrl(id: string): string {
    return this.api.getDownloadUrl(id);
  }

  parseMeta(metaJson: string): any {
    try {
      return JSON.parse(metaJson);
    } catch {
      return {};
    }
  }

  getDimensions(item: WorkshopItem): string | null {
    if (item.type === 'set') return null;
    const meta = this.parseMeta(item.meta_json);
    if (meta.width && meta.height) {
      return `${meta.width}×${meta.height}`;
    }
    return null;
  }
}
