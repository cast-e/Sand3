import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterLink } from '@angular/router';
import { ApiService, WorkshopItem } from '../../core/services/api.service';

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

      <br>

      @if (loading()) {
        <div class="loading-state">
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
                  <span class="badge" [class.badge-blue]="item.type === 'set'" [class.badge-green]="item.type === 'save'" [class.badge-yellow]="item.type === 'stamp'">
                    {{ item.type | uppercase }}
                  </span>
                  <a [routerLink]="item.type === 'set' ? ['/workshop/sets', item.id] : ['/workshop/items', item.id]" class="item-title-link">
                    {{ item.title }}
                  </a>
                </div>
                <span class="author-tag">by {{ item.author }}</span>
              </div>
              <div class="card-body">
                <p class="item-desc">{{ item.description || 'No description provided.' }}</p>
              </div>
              <div class="card-footer item-actions">
                <a [href]="getDownloadUrl(item.id)" class="btn btn-sm btn-primary">
                  <img src="/icons/arrow_down.png" class="silk-icon" alt="Download" /> Download
                </a>
                <button class="btn btn-sm active" (click)="removeFavorite(item)">
                  <img src="/icons/star.png" class="silk-icon" alt="Star" /> Remove
                </button>
              </div>
            </div>
          }
        </div>
      }
    </div>
  `,
  styleUrl: '../workshop/workshop.scss'
})
export class FavoritesComponent implements OnInit {
  private api = inject(ApiService);
  items = signal<WorkshopItem[]>([]);
  loading = signal<boolean>(true);

  ngOnInit() {
    this.api.getUserFavorites().subscribe({
      next: (favs) => {
        this.items.set(favs);
        this.loading.set(false);
      },
      error: () => this.loading.set(false)
    });
  }

  removeFavorite(item: WorkshopItem) {
    this.api.toggleFavorite(item.id).subscribe(() => {
      this.items.update((list) => list.filter((i) => i.id !== item.id));
    });
  }

  getDownloadUrl(id: string): string {
    return this.api.getDownloadUrl(id);
  }
}
